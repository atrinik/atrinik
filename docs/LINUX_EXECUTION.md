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
