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
python3 -m atrinik_workspace.linux_platform --desktop wayland --gpu mesa --render-device /dev/dri/renderD128 --audio
python3 -m atrinik_workspace.linux_platform --desktop x11 --gpu nvidia --audio
```

The render-node number above is an example, not a fixed assignment. X11 requires
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

## Portable client export

The public exporter runs in the exact published portable build environment,
`ghcr.io/atrinik/classic-portable-build@sha256:df72e2ece5edeaee584a1b8eb30e523c6154a0adae7a1fea5e954ed6bc9dbae1`.
It is a build input, not a delivery coordinator. Its current full-commit consumer
guard requires Classic `4998131ad2ae4c9680685fd87e2d85de1dc15fd9`.
Use a clean Classic profile with verified released sound matching its selected
sound source commit, and a new absolute destination
whose parent is owned by the invoking user and not writable by other users:

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
