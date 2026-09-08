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
`0 0 4294967295`. Caller numeric-PID mountinfo must agree with genuine procfs
and filesystem device identities. PID1 must report root UID and systemd; installed
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

## Portable binary evidence

`linux_export.inspect_elf` examines a leased, already-open regular ELF descriptor
with bounded, time-limited GNU readelf output; it never runs the payload or ldd.
It records class, endianness, machine, interpreter, static dependencies, runtime
search paths and version-needed records by provider. The structural dependency
report requires explicit bundled providers, materialized SONAME filenames and
host libraries, and rejects incompatible objects or escaping search paths.
The producer must configure the reported loader library directories explicitly.

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
