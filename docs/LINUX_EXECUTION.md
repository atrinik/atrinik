# Linux execution and authority

Changes to this contract do not authorize their own delivery. Until a maintainer
accepts the synchronized implementation, proof and guidance, that delivery keeps
the previously accepted canonical-container boundary. A proposed patch, fixture
result, environment flag or issue description cannot activate native authority.

| Host / role | Display | Delivery authority | Evidence |
| --- | --- | --- | --- |
| Ubuntu 24.04/26.04 or Debian 12/13, direct systemd host or VM | Optional | `native-linux` after complete live proof | Distribution-specific native tests required |
| Linux-hosted pinned coordinator | None required | `canonical-linux` after complete live proof | Pinned Ubuntu 26.04 image and per-session proof |
| Optional native Linux client container | Selected X11/Wayland and real GPU | Runtime capability does not grant delivery authority | Selected renderer and gameplay evidence |
| Windows/WSL2/WSLg pinned coordinator | WSLg optional in explicit desktop config | Canonical container proof only | Keep WSLg and native Windows evidence separate |
| Windows cross-build container | None | No ledger authority | MXE package/build evidence only |
| Native Windows | Native graphics for D3D12 qualification | No Linux ledger authority | Native Windows runtime evidence |

The version list is the implementation's supported systemd distribution contract,
not a claim that every version has hardware or integration qualification. Record
actual test coverage separately; never infer Debian or older Ubuntu coverage from
the pinned Ubuntu image or synthetic fixtures.

The direct-host probe reads bounded trusted OS files through no-follow descriptor
traversal, accepts the actual passwd home/user instead of a fixed UID, and requires
private user-owned Codex state. Repository, home, Codex and existing mutable roots
must use native ext4, XFS, Btrfs, ZFS or tmpfs with trusted owner/mode ancestry.
World/group-writable ancestors, links, network/bridge filesystems and missing
proof fail closed. A missing build/review descendant does not grant reuse:
the existing helper creates it only through its own pinned-parent transaction.

Both caller and PID1 UID/GID maps must contain the full initial identity row
`0 0 4294967295`. Caller numeric-PID mountinfo must identify genuine procfs
and supported filesystems at the checked paths. PID1 must report root UID and systemd; installed
systemd and `/run/systemd/system` must be trusted, root-owned and non-writable
by other users. WSL and container signals reject direct-native classification.
Ordinary-user denial of PID1 executable/namespace links is not a request for
privilege escalation and does not itself reject a supported host.

Trust includes the kernel, host administrator and executing user/code. This
recognizes an ordinary supported host configuration; it does not attest PID1
executable bytes, resist privileged host administration, or distinguish every
administrator-created container/chroot. Initial UID/GID mapping checks reject
ordinary rootless ownership forgery. Environment flags and systemd names alone
never establish authority.

After context proof, unchanged authenticated issue/PR ownership, ignored review
root inventory, dedicated safe worktree binding, no-follow Git/source proof,
CAS, ordered leases, collision checks and recovery rules all still apply.
Parallel sessions isolate Codex/cache/worktree/state coordinates. Existing
canonical containers continue without replacement or remount.

Before repository work, run `python3 -m atrinik_workspace.linux_platform` for
tools and Git LFS filters. Add `--docker` only for Docker operations. Missing
Docker permission is independent of graphics availability. Tool presence is not
a proof of complete native build compatibility; run the owner-required build and
test checks on the selected distribution.


## Native prerequisites and terminal selection

On the supported Debian/Ubuntu systemd distributions, install the native wrapper
and source-build front-end dependencies through the distribution package manager:

```sh
sudo apt-get update
sudo apt-get install python3 python3-venv git git-lfs cmake ninja-build pkg-config
git lfs install --skip-repo
python3 -m atrinik_workspace.linux_platform
```

This installs orchestration tools, not the complete Classic C library toolchain.
Classic CMake enforces its owner-declared development-library versions; the
qualified portable producer supplies those exact versions for export. Do not
replace its SDL3 libraries with SDL2 or claim a source build from tool presence.
An exported client needs no compiler or development headers. Mesa hosts need the
distribution's `mesa-vulkan-drivers`; NVIDIA hosts need the installed proprietary
driver's matching Vulkan ICD. Preserve a working host driver. The portable image
contains the Vulkan loader, not graphics drivers. Native client execution does
not require Docker; a client container additionally requires a working daemon
and, for NVIDIA, the NVIDIA Container Toolkit.

After the native authority contract is merged and accepted, use the actual
passwd user's private home/Codex directory and the standard GitHub credential
store described in [coordinator authentication](COORDINATOR_AUTH.md). Run the
public coordinator probe before issue/project preparation. Reconnect to the same
host, user, worktree and ledger; rerun live context, authenticated actor,
complete collision inventory and the helper's target/CAS/lease proof. A saved
success document never authorizes reconnect. During delivery of a change to this
contract, keep the previously accepted canonical container.

## Fresh headless containers and child reaping

After the existing image-access and isolated-session prerequisites in README,
select the Linux headless configuration explicitly from the host terminal:

```sh
devcontainer up --workspace-folder "$HOST_REPO" --config "$HOST_REPO/.devcontainer/devcontainer.json"
```

Use the exact container ID returned for that owned session. Before attaching,
inspect that same container's creation setting:

```sh
docker inspect "$CONTAINER_ID" --format '{{.Id}} {{.HostConfig.Init}}'
```

The Linux configuration sets `init: true`, which makes the Dev Containers CLI
pass Docker's `--init` option when creating a new container. Docker's small init
process forwards signals and reaps orphaned child processes. This avoids relying
on a long-lived shell or `sleep` as PID1 to reap adopted descendants. It does not
replace the wrapper's ownership, supervised shutdown or final process/holder
checks, and does not prove that all application processes have stopped.

This creation setting adds no display sockets, GPU devices, audio endpoints or
new delivery authority. Continue with the existing exact-container coordinator
probe and terminal workflow before repository work. Configuration parsing and
wrapper tests do not establish a fresh-container lifecycle result; record that
smoke evidence separately on a newly reserved isolated session.

An already running container retains its original creation settings. Attaching
or restarting it does not add an init process. Preserve its exact identity,
mounts, worktree and ledger; do not remove, recreate or remount it to apply this
setting. A retained session without init continues under its existing bounded
child-reaping and shutdown procedures until a separately owned fresh session is
authorized and verified. Windows/WSLg and cross-build configuration behavior is
unchanged by this Linux-headless setting.

## Explicit Linux desktop container capabilities

On the selected Linux desktop, the standalone module prints a JSON Docker
argument array without launching anything. Choose the active session and a
render node discovered on that host, for example:

```sh
python3 -m atrinik_workspace.linux_platform --desktop x11 --gpu mesa --render-device /dev/dri/renderD128 --audio
python3 -m atrinik_workspace.linux_platform --desktop x11 --gpu nvidia --audio
```

The render-node number above is an example, not a fixed assignment. The portable
client uses X11; on a Wayland desktop select its actual XWayland `DISPLAY` and
`XAUTHORITY`. The separate `--desktop wayland` capability is only for a client
that actually implements that backend. X11 requires
the actual private `XAUTHORITY` file and local `DISPLAY`; Wayland requires the
active `XDG_RUNTIME_DIR` and `WAYLAND_DISPLAY`. Audio is an explicit Pulse socket
option, including PipeWire's Pulse compatibility service where present. The
output uses the caller's actual UID/GID and selected device's group, read-only
session socket/cookie mounts, and no global X-server access grant. X11 also sets
the container hostname to the observed desktop hostname so FamilyLocal cookie
records select the same hostname/display rather than Docker's generated name. A selected
NVIDIA runtime additionally needs the host NVIDIA Container Toolkit and a
compatible driver; emitted arguments do not establish that those are installed.

Check the process status before parsing stdout. Desktop failure returns 2 with
an error on stderr and no success document. Pass successful `docker_arguments`
as an argument array to a Docker invocation, never through shell `eval`. Add the
accepted immutable image, isolated client configuration/state and explicit
server endpoint through the runtime workflow. Run `--docker` separately to
check daemon/user permission; that check says nothing about display or GPU.
Actual selected-renderer evidence and interactive gameplay remain required.

A terminal can consume the result without interpreting shell text. For example,
a separately owned runtime bootstrap script can run the probe with
`subprocess.run([...], check=True, capture_output=True, text=True)`, decode its
stdout with `json.loads`, then invoke `subprocess.run(["docker", "run",
*result["docker_arguments"], ...], check=True)`. Include explicit owned name,
read-only client payload, separate writable config, and pinned image arguments
in that same array. Never use `eval`, `xhost +`, privileged mode, the Docker
socket inside the client, or automatic selection of another user's display.

## Portable client export

The public exporter runs in the exact published portable build environment,
`ghcr.io/atrinik/classic-portable-build@sha256:df72e2ece5edeaee584a1b8eb30e523c6154a0adae7a1fea5e954ed6bc9dbae1`.
It is a build input, not a delivery coordinator. Before merge, the automatic
`Linux portable acceptance` pull-request workflow is the supported actual
producer route; the retained canonical container has no Docker bridge. Its
`Portable client build and relocation` job is nonpublishing, limits the producer
to two CPUs, and serializes heavy runs without cancelling another owner's run.
The `linux-portable-HEAD_SHA` artifact retains the movable client, exact source
and release identities, configure/compiler evidence and relocation results for
seven days. Download and retain evidence needed for the delivery before expiry.
The final runtime process has neither credentials nor original source/build
mounts, and its network, display, GPU and audio endpoints are absent.

Its current full-commit consumer
guard requires Classic `4998131ad2ae4c9680685fd87e2d85de1dc15fd9`.
Use a clean Classic profile with verified released sound matching its selected
sound source commit, and a new absolute destination
whose parent is owned by the invoking user and not writable by other users.
Create `classic-released-audio` with the exact v1.0.0 coordinates in the
[released sound recipe](../README.md) before running these producer
commands (the CI driver creates its own equivalent saved profile):

```sh
mkdir -p "$HOME/.local/opt"
chmod 700 "$HOME/.local/opt"
./atrinik linux export --profile classic-released-audio --output "$HOME/.local/opt/atrinik-client"
./atrinik linux verify "$HOME/.local/opt/atrinik-client"
```

Both commands emit one JSON result on success and route diagnostics to stderr.
Export refuses an occupied destination. It verifies immutable Git/LFS source
payloads under the existing profile/source leases, uses a separate producer-bound
build cache, and retains the build lease through source revalidation and atomic
publication. A failed operation preserves its private `.atrinik-export-*` staging
for diagnosis and does not replace an existing output.

The complete installed producer closure is bound to an independently reconstructed
OCI overlay inventory: 299 regular files, including all eight upstream archives,
222 Debian source archives, build recipes and notices. Three pinned aggregate
SHA-256 values cover the producer tree, 145 Debian copyright paths and 14 common
license texts. Each canonical JSON map uses sorted keys and compact separators;
producer/common-license keys are relative paths and values are
`sha256`, `size`, `executable`. Debian keys are requested absolute copyright paths
and values additionally contain the resolved absolute path. Missing, extra,
nonregular, changed or mode-different producer files reject publication; source
copies must retain the exact checked inventory. Hashes change only with a newly
qualified immutable producer, never to accept local corruption.

The directory contains the client, materialized media, application libraries,
OpenSSL provider, corresponding sources, producer recipes and notices. ELF
inspection checks actual provider hashes, static dependencies, symbol/version
providers and declared dynamic SONAMEs. The host supplies glibc 2.36 or newer,
its loader and graphics drivers. The X11/Vulkan producer uses XWayland on a
Wayland desktop; PulseAudio or PipeWire's Pulse service supplies audio.

Move the whole directory, then run its `atrinik` launcher. It sets library and
OpenSSL-provider paths relative to itself and keeps mutable configuration outside
the output. Choose `ATRINIK_CONFIG_DIR` explicitly for isolated clients.
`linux verify` checks exact bytes, modes and inventory after relocation; actual
loader, media-decoding, connectivity and per-host qualification remain separate
acceptance results. Hardware gameplay and audible playback need their own proof.

## Independent headless server and native client

Use an isolated server workspace and container, with no display/audio mounts or
GPU device. Publish its chosen UDP port at container creation; attach/reconnect
cannot add a Docker mapping. For a same-host desktop, the mapping is
`--publish 127.0.0.1:17300:17300/udp`. A remote desktop requires an explicitly
chosen reachable address and corresponding firewall rule. A localhost mapping
must not be advertised as remotely reachable.

The terminal bootstrap may create a private ignored configuration from the
headless configuration before `devcontainer up`. In the already reserved fresh
server workspace, set only the explicit port mapping:

```sh
python3 - <<'BOOTSTRAP'
import json
from pathlib import Path
source = Path('.devcontainer/devcontainer.json')
config = json.loads(source.read_text())
config['runArgs'] = ['--publish', '127.0.0.1:17300:17300/udp']
target = Path('build/linux-server-devcontainer.json')
target.parent.mkdir(mode=0o700, exist_ok=True)
with target.open('x') as stream:
    json.dump(config, stream, indent=2)
BOOTSTRAP
devcontainer up --workspace-folder "$HOST_REPO" \
  --config "$HOST_REPO/build/linux-server-devcontainer.json"
```

Complete the existing isolated Codex/cache/auth mount setup before bootstrap;
this recipe does not permit sharing mutable session state. Preserve the exact
returned container ID and prove its image, mounts and headless context before
working inside it. An existing container keeps its creation settings and must
not be replaced or remounted to apply this example.

Inside that owned container, prepare and start only the server with registered
persistent state. No client or audio device is needed:

```sh
./atrinik init classic-server content resources --jobs 2
./atrinik state add linux-review
./atrinik up --name linux-headless --profile classic \
  --state linux-review --service server --port 17300
./atrinik ps linux-headless --json
./atrinik logs linux-headless server --tail 100
```

Wait for the exact topology's ready status. Hand off only `endpoint.port` and
`endpoint.fingerprint` from its current `ps --json` result plus the reachable
host address. The fingerprint is the 64-hex SHA-256 certificate identity; never
copy the private QUIC key, supervisor control socket, account password or state
files into a public artifact.

Move and verify the client export on the desktop host. Use a private mutable
configuration outside the export and pass the complete authenticated QUIC tuple
as one argument:

```sh
install -d -m 700 "$HOME/.local/state/atrinik-client/linux-review"
ATRINIK_CONFIG_DIR="$HOME/.local/state/atrinik-client/linux-review" \
  /absolute/path/to/atrinik-client/atrinik \
  --server="SERVER_HOST 17300 SERVER_FINGERPRINT" --stun_server=off --nometa
```

First exercise rejection using a separately recorded tuple with one fingerprint
hex digit changed. It must fail authenticated connection without logging in.
Then select the verified tuple, log in interactively and record the actual
selected Vulkan device, gameplay, and audible sound separately. An image/font
load, PCM decode, software renderer or silent audio sink cannot replace those
hardware results. Do not send local scenario passwords into durable logs or
handoff text.

For persistence, create a player or make an observable gameplay change, stop the
client, then stop the exact server through the wrapper:

```sh
./atrinik down linux-headless
./atrinik ps linux-headless --json
./atrinik up --name linux-headless --profile classic \
  --state linux-review --service server --port 17300
./atrinik ps linux-headless --json
```

Reconnect using the fresh endpoint result and verify the saved player/change.
After final client shutdown, run `down` again and verify no live topology before
stopping only the exact owned container. Do not signal saved PIDs, delete lease
or control files, or use cleanup as shutdown. Keep the persistent state and
verification evidence. Windows/WSLg, MXE and native Windows D3D12 results remain
separate records.

## Portable binary evidence

`linux_export.portable_metadata_report` checks the six published portable metadata
records against hashes and immutable OCI coordinates supplied by the caller's
separate authenticated producer/registry verification. It accepts only the
Linux/amd64 Debian 12/glibc 2.36 portable target, checks the installed contract,
package and CPU declarations, resolves reported library/plugin edges, checks
versioned-symbol provider records and compares source/shader coordinates.
Duplicate, malformed, missing, disconnected or contradictory records fail.
Distinct SONAME declarations may share a dynamic feature name, as SDL does for
X11/Vulkan. Provider paths are aggregated by feature and can repeat when
distinct declarations resolve to the same object; repeated identical
declarations fail. Exclusions match exact object, feature and SONAME-list
declarations; they do not exclude other declarations sharing that feature.
The metadata lacks a SONAME-to-resolved-path map, so
`dynamic_soname_resolution_verified` remains false; reported paths do not prove
that every requested SONAME resolves.

The requested Classic commit must equal both the contract's consumer commit and
the shader source commit. The published consumer enforces this full-commit guard;
identical client/shader files at a different repository commit do not satisfy it.
A replacement producer contract requires its own review and publication.

This adapter only returns `metadata_consistent`. It does not fetch an image,
execute its tools, inspect its filesystem paths, verify registry provenance,
prove a source checkout/lease, rehash runtime binaries/source archives, validate
actual symbol definitions or establish legal/runtime/hardware qualification.
Caller-provided hashes are not self-authenticating. Keep those independent proofs
with the immutable image handoff and bind actual copied payload bytes during
later export integration. The image remains a build input, never coordinator
execution authority. Its X11/Vulkan client supports a Wayland desktop only via a
separately qualified XWayland setup; no native Wayland backend is claimed here.

`linux_export.inspect_elf` examines a leased, already-open regular ELF descriptor
with bounded, time-limited GNU readelf output; it never runs the payload or ldd.
It records class, endianness, machine, interpreter, static dependencies, runtime
search paths and version-needed records by provider. The structural dependency
report requires explicit bundled providers, materialized SONAME filenames and
host libraries, and rejects incompatible objects or escaping search paths.
The producer must configure the reported loader library directories explicitly.

Version definitions are parsed with unique names/indexes and complete auxiliary
parent/count records. Each bundled provider must define every version name its
resolved consumers require; a same-SONAME replacement with missing definitions
is rejected. BASE object names are not provided symbol versions. The report
records checked requirements and leaves host-library requirements explicitly
unverified, because their actual providers have not been inspected.
`provider_version_names_verified` describes only this version-name comparison.
It does not prove that individual symbols exist with the required versions;
`symbol_versions_verified` remains false. No version comparison establishes
source authority or replaces qualification against the actual host loader.

This report does not prove source provenance, symbol-version compatibility,
`dlopen` plugin closure, legal notices, or actual loader/hardware operation.
The source owner must bind it to the same copied bytes under its active lease.
A manifest supplied by a caller is evidence only. In particular, the pinned
Ubuntu 26.04 image uses glibc 2.43; moving its output does not demonstrate that it
runs on older supported native build hosts. Record required loader/provider
versions and qualify the exported application on each supported runtime baseline.

See the [GNU readelf reference](https://sourceware.org/binutils/docs/binutils/readelf.html)
for the inspected header, program-header, dynamic and version sections.


The independent `copy_payload` primitive accepts already-open source and exclusive
staging descriptors, copies bounded chunks, verifies the actual written digest,
and detects source/staging changes. It rejects Git LFS pointers and occupied
staging files. Only the authenticated source owner can provide the expected
record while holding its source/build lease; that owner must revalidate the
source inventory and staged destination before atomic publication. The primitive
never publishes or grants provenance. Failures leave the caller-owned staging
file for that owner's recovery workflow.

The launcher generator uses a relative `bin/atrinik` and
`share/games/atrinik` layout, supplies the export's `lib` directory, and starts
from the data directory supported by Classic's current-directory lookup.
`ATRINIK_CONFIG_DIR` selects separate persistent state; otherwise the launcher
uses `XDG_STATE_HOME/atrinik-client` or `HOME/.local/state/atrinik-client`.
State inside the payload is rejected. It requires the standard Linux shell and
coreutils, including `realpath`. Launcher fixture tests prove relocation and
argument/state handling only; they do not replace real client or media acceptance.
