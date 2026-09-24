# Native Linux split-runtime qualification

This record tracks the native Linux acceptance required by
[#604](https://github.com/atrinik/atrinik/issues/604) for
[#562](https://github.com/atrinik/atrinik/issues/562). Qualification is incomplete:
the portable artifact passed independent verification, but the first native
connection attempt timed out before certificate verification. Hardware gameplay,
audible playback, successful login and persistence across restart are not yet
accepted.

## Verified inputs

| Input | Revision or immutable image |
| --- | --- |
| Wrapper used for current verification | `57f2a3540802f53eaee1d6be2ac4b4db82d1588b` |
| Original portable artifact producer wrapper | `b519a1baa39fe1ae498545acf3db65d84058f01f` |
| Classic | `4998131ad2ae4c9680685fd87e2d85de1dc15fd9` |
| Content, `main` | `27c63b969cad8739fd63ed523a33456f7a1595b0` |
| Resources | `bff12778a2181b15c12393b63e4761fd568c8693` |
| Sound | `d0561bf9ff8dc88836818dbe602a5a256c6c0e3f` |
| Devcontainer source | `aa7e944ec3afcf2eaec434b164618680d514a77b` |
| Server/coordinator image | `ghcr.io/atrinik/linux-build:1.10.0@sha256:7904a1802054662b0ede5b55de72e4c92b0112a3c211125f994ed6c62e9ec9d8` |
| Portable producer/verifier image | `ghcr.io/atrinik/classic-portable-build:1.12.0@sha256:df72e2ece5edeaee584a1b8eb30e523c6154a0adae7a1fea5e954ed6bc9dbae1` |

The retained archive is 1,390,571,520 bytes with SHA-256
`c3b8e64b3e6d55f978706563328df3f3f7b88c227a9398c343d595a4c80e6b07`.
Its producer revision remains historical. The accepted export, portable-input
and relocation-verification code and CI recipe were unchanged between that
producer and the wrapper used for this verification. This is verification of
the retained artifact, not a claim that it was rebuilt at the current wrapper
revision.

## Portable artifact results

An independent reviewer rehashed the archive and every installed payload file,
checked immutable source/provider relationships and legal notices, and inspected
the actual verifier container. The archive had one export root, 3,168 members
(297 directories and 2,871 regular files), and no links, special files, duplicate
paths, traversal entries or set-ID modes.

After extraction, the export directory was moved. The accepted
`./atrinik linux verify EXPORT` check passed. The isolated CPU verifier mounted
only the relocated export read-only, four accepted verification files read-only,
and its private output directory. It had no network, source checkout, coordinator
state, credentials, display, GPU or audio device. Original source/build locations
were unavailable. The verifier exited successfully.

| Check | Result |
| --- | --- |
| Installed inventory | 2,870 inventoried payload files |
| Legal material | 192 notices, corresponding sources and export recipes verified |
| Application library closure | 83 application libraries; 85 loader-listing and loader-trace paths |
| Image decoding | 125 images decoded |
| Font loading | 8 fonts loaded |
| Audio decoding | 339 positive PCM-prefix decodes |
| OpenSSL providers | Default and legacy providers loaded |
| Launcher | Relocated launcher help invocation succeeded |

Audio decoding does not establish complete-track playback or audibility.
The CPU verifier does not establish hardware rendering or gameplay. The native
export subsequently had owner-write bits removed while preserving executable
bits, and `linux verify` passed again. This protects ordinary writes; it is not
a claim that the native user process runs in a filesystem security sandbox.

## Server isolation and first native attempt

The server ran in its independently owned execution container under an
unprivileged user, with all capabilities dropped and no-new-privileges enabled.
Its two mounts were the reserved workspace and its separate build cache.
Fresh inspection established that coordinator authentication, private delivery
state, Codex files, private keys, display/audio sockets and GPU devices were
absent. The coordinator and executor saw the same physical Git, source, worktree,
state and lease objects. Docker published only host
`127.0.0.1:17300/udp`.

The public server lifecycle used profile `issue604-classic-profile`, topology
`issue604-split-server`, scenario `issue604-basic-player` and registered state
`scenario-issue604-basic-player`. The state belongs to the scenario. No player
or account files were handcrafted.

The server reached ready status using runtime build
`issue604-classic-profile-4000b7325b12`. Historical 54/54 CPU-test evidence
belongs to the distinct build `issue604-classic-profile-d98afdaf4fc3`; it is
not attributed to this runtime build.

The native attempt used the verified exported executable and separate private
configuration on Ubuntu 26.04.1 with a Wayland desktop. The process received a
fresh server endpoint with exactly one certificate-fingerprint hex digit changed,
server-only automatic connection selection, disabled STUN discovery and
`--nometa`. No account, password or scenario secret was supplied. Its environment
was restricted to explicit user, locale, desktop-routing and client-config
selectors. Ordinary native user filesystem access remained available.

A separate supervisor enforced a 60-second attempt deadline, at most 10 seconds
of cleanup, and a combined retained-output limit of 4 MiB. Process identity was
bound to the actual exported executable, PID and process start time. The process
exited normally with code 0 after 50.58 seconds. Its QUIC handshake timed out;
the log contained no certificate-fingerprint mismatch. **This is a failed
acceptance attempt, not evidence of wrong-fingerprint refusal.**

The immediate cause was an execution-namespace mismatch: the server's packaged
configuration listens on container `127.0.0.1` and `::1`, whereas Docker's
published UDP port addresses the container's bridge interface. At the verified
wrapper revision, supervised `up` exposes no listener override. Qualification
therefore requires a separately owned supported supervised-listener correction.
Editing generated runtime configuration or silently replacing the topology with
a foreground server would not validate the intended `up`/`ps` lifecycle.

After the attempt, the client was confirmed stopped. Public `down` completed
cleanly, `ps` recorded released runtime/process/port leases, and the exact idle
executor was stopped. Registered scenario state and evidence were preserved.

## Remaining acceptance

1. Apply the accepted supervised-listener correction, retaining host-only UDP
   publication and the existing isolation boundary. Refresh source coordinates,
   runtime observations and affected validation.
2. Repeat the wrong-fingerprint check. Require the explicit mismatch message
   with its expected value equal to the deliberately changed pin and its actual
   value equal to the fresh server endpoint fingerprint. A timeout is failure.
3. Connect using the actual endpoint and a supported scenario or interactive
   account flow. Keep disposable credentials out of arguments, logs and public
   evidence.
4. Make an observable supported gameplay change, log out cleanly, stop and
   restart the exact server using the same registered state, reconnect using
   its fresh endpoint, and verify the saved change.
5. Bind evidence of the actual hardware renderer to the native client process
   and observe gameplay. Desktop GPU enumeration alone is insufficient.
6. Verify audible playback separately from decoding and audio-dispatch logs.
   Request the small human observation only after independent checks pass.
7. Complete final-head wrapper checks and independent review of this document.
   Publish only sanitized results; preserve local state and private evidence.

See [Linux execution](LINUX_EXECUTION.md) for the portable artifact contract and
supported runtime lifecycle. Linux results do not qualify WSLg or native Windows.
