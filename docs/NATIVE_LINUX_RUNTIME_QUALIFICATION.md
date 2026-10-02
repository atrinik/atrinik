# Native Linux split-runtime qualification

The native Linux acceptance for
[#604](https://github.com/atrinik/atrinik/issues/604) and
[#562](https://github.com/atrinik/atrinik/issues/562) was completed on
2026-10-01: a relocated, verified Classic client connected to a separate
credential-free headless server; the maintainer played, heard sound, and
reconnected to the same character after a supported server restart.
Process observations made during a connected session corroborated hardware GPU
and audio use. Byte verification, runtime behavior and human observations have
separate evidence below.

This document also records the retained delivery's actual recovery and immutable
dependency advances. The October qualification used independently owned parent
acceptance resources; it does not relabel the older retained #604 runtime as the
new runtime or transfer its state.

## Qualified inputs

| Input | Exact revision |
| --- | --- |
| Wrapper used for October builds and runtime | `8bb4dd7c52001a85ee64b7fd3c306adb1d6205e9` |
| Accepted wrapper base for this evidence delivery | `8e300f4bec2469635ee6765f5efdb43fbc5fdcfd` |
| Classic | `d926f6fd0418fb1af9060158c43d8d3ff5252580` |
| Content, `main` (server input) | `d5fc39f1dcb8b031b3302d499c59975a548849b9` |
| Resources (server input) | `bff12778a2181b15c12393b63e4761fd568c8693` |
| Released sound, `v1.0.0` | `d0561bf9ff8dc88836818dbe602a5a256c6c0e3f` |
| Portable producer source | `960b41ced51418892f18fc157373d89444c843db` |

The CPU server image was
`ghcr.io/atrinik/linux-build:1.10.0@sha256:7904a1802054662b0ede5b55de72e4c92b0112a3c211125f994ed6c62e9ec9d8`.
The portable producer was
`ghcr.io/atrinik/classic-portable-build@sha256:8f6d345f0e24afad5e53d9e35c37f3e3b4012e5cc2c399334711e55e61a2a338`,
with Linux/amd64 platform manifest
`sha256:7b7f36fef585c5424a56edbe2939a25ab1facbb3dd17a2f3a55980866dd85486`.

The portable input record binds Classic and the verified released sound.
Content and resources are independently checked server inputs; they are not
claimed as inputs consumed by the portable client build. The subsequent wrapper
merge #635 changes required-check observation, not these application inputs or
the export/runtime implementation. Independent review accepted reuse of the
October runtime evidence after that merge.

## Portable bytes, relocation and media

The October export was produced in the pinned CPU image, then moved to a
directory containing a space. Its manifest SHA-256 is
`17996d9e04a1e228ae28f5be7b83a69287f286e3d915636bc005aea36f4d1637`;
the client executable SHA-256 is
`5921947b1d864ab2fa5df420a1fc6204b90eda4058ff2b607fcb2c4abeb90cdd`.
The released sound archive was verified against SHA-256
`e3f17d314b3933db9c6af3f9290c5df79375a6cc3d570ea29f225b53de362784`.

The isolated relocation verifier had no network, display, GPU, audio or
credentials. Only the export, verification controls and evidence output were
mounted; producer prefixes were masked and original source/build paths were
unavailable. The export and verifier both exited successfully. Mutable client
configuration remained outside the moved export.

| Verification | Observed result |
| --- | --- |
| Installed payload inventory | 2,872 files verified |
| Legal material | 192 notices; corresponding sources and producer recipes materialized |
| Library closure | 83 application mappings; 85 loader-listing and 85 loader-trace paths |
| Images and fonts | 125 image decodes; 8 font loads |
| Released audio | 339 positive PCM-prefix decodes |
| OpenSSL | Default and legacy providers loaded |
| Relocated executable | Help invocation succeeded |

These audio checks establish nonempty decoded prefixes, not complete-track
listening. The separate maintainer observation below establishes audibility.
Loader verification accepts only the documented host-library boundary; it does
not claim independence from the operating system's graphics drivers.

## Headless server and authenticated connection

All application compilation and CTest execution ran in pinned CPU containers.
The October server build passed all 54 tests, including protocol, Python/plugin
and server asset-path tests. A preliminary immutable-dependency acquisition
failed on bridge DNS; the preserved retry used host networking for acquisition
only. The gameplay server subsequently used normal bridge networking with
explicit host-loopback UDP publication.

The headless execution container ran unprivileged without display, GPU, audio,
GitHub authentication, private keys, coordinator state or Docker-socket mounts.
The supported `up`, `ps`, `logs` and `down` lifecycle reached server readiness
and then clean shutdown with released process, runtime, state and port leases.

For the separate-client qualification, the server topology was
`project562-hw-bridged-20261001`, using persistent state
`project562-hw-state-g`. Its verified endpoint was:

| Endpoint field | Observed value |
| --- | --- |
| Address | `127.0.0.1` |
| Published UDP port | `17306` |
| Certificate fingerprint | `ac268da69a450fd2862211f88476b0d78bbc8246770791136074a54a85ac2893` |

A deliberately changed first hex digit produced the explicit
`QUIC certificate fingerprint mismatch` diagnostic: expected `0c268…`,
actual `AC268…`. The complete 64-hex values, compared without case sensitivity,
match the deliberately altered pin and the fresh server endpoint respectively.
A timeout or generic “server offline” UI message was not treated as rejection
proof. The correct endpoint subsequently allowed account creation and login
through the normal client interface. No credential values are included in this
record.

The supported listener choice was `--server-listener all-ipv4` inside the
container; Docker's host publication remained loopback-only. This distinction
allows a bridge-published UDP endpoint to reach the server without exposing it
on every host interface.

## Persistence, hardware gameplay and audible playback

The maintainer created an account, logged in, played, heard sound and exited.
The server was stopped and restarted using the same persistent state. The
maintainer then logged into the same character and exited. This proves the
observed account/character persistence; it does not claim a comparison of
unrecorded inventory, map coordinates or every saved field. No account or player
save files were handcrafted.

Server login/logout observations bracketed the first session at
20:06:09–20:06:33 UTC and the post-restart session at 20:10:19–20:10:30 UTC on
2026-10-01. The restarted runtime generation was
`6a22f4d44a798de11a5eb36490614a8c7b7ecbc0135602057109ef5a7286183e`.

A further connected session at 20:24:29–20:25:08 UTC aligned the native exported
executable with live per-process NVIDIA GPU use (522 MiB), open
`/dev/dri/renderD128` and `/dev/dri/renderD129` descriptors, and a PulseAudio
descriptor. Together with the production client's hardware requirement and
the maintainer's gameplay observation, this qualifies the actual hardware
path. Desktop device enumeration or a software-renderer test alone would not.
Audibility is the maintainer's separate observation, not an inference from
decoding, a Pulse descriptor or audio dispatch logs.

The container-client recipe was also exercised, but failed with
`No supported SDL_GPU backend found`. Container GPU gameplay is therefore
unavailable on this tested configuration. The successful route is the native
hardware client with the separate headless container server. No Windows, WSLg
or native Windows D3D12 qualification is implied.

The client exited and the owned server was stopped through the wrapper with
`shutdown.clean=true`; its leases were released and persistent state retained.
There was no deployment, cleanup, release or foreign-runtime takeover.

## Historical evidence and retained recovery

The earlier portable qualification remains attributed to Classic
`4998131ad2ae4c9680685fd87e2d85de1dc15fd9`, producer source
`aa7e944ec3afcf2eaec434b164618680d514a77b`, and image digest
`sha256:df72e2ece5edeaee584a1b8eb30e523c6154a0adae7a1fea5e954ed6bc9dbae1`.
Its historical producer wrapper was
`b519a1baa39fe1ae498545acf3db65d84058f01f`; the verification controls were
byte-identical at the later verification wrapper `57f2a354…`.
The archive SHA-256 was
`c3b8e64b3e6d55f978706563328df3f3f7b88c227a9398c343d595a4c80e6b07`.
Independent verification covered all 2,870 inventoried payload files,
corresponding sources/notices, relocation, providers and media decoding.
This satisfies the historical current-source obligation for that release.
The October `d926f6fd…` export is a separately built and verified accepted
successor; the two source revisions are not interchangeable.

The first retained native attempt timed out because the server listened only
on the container loopback interface. That failed attempt remains a failure.
After the merged supervised-listener correction, a later bounded attempt
recorded explicit wrong-pin rejection. A subsequent historical login reached
the game but logged protocol rejections and reconnects; that session was accepted
only as limited connectivity evidence, not stable gameplay. Current-input
October evidence supersedes those incomplete gameplay claims without rewriting
the original logs.

The retained #604 delivery used the accepted public recovery operations:

- The original erroneous state observation was retired while preserving its
  immutable intent and producer bytes. The actual scenario-owned registered
  state remained `scenario-issue604-basic-player`; no false alias was created.
  The corrected resource namespace stayed bound to the dedicated worktree.
- The Classic dependency declaration, complete plan and tested-build stages
  advanced only the declared source from `4998131…` to `d926f6fd…`.
  The separate build `fc6dd05d5b63` passed 54/54 tests.
- The content-plan and content-built stages admitted read-only
  `content@main` at `d5fc39f1…`, containing merged content PR #265.
  A separate build `e8d02723fd4d` passed 54/54 tests under plan
  `6c46c9ed8e5ff5be0a55b5ee367dcccaadac0b96bac302c912737fc2bfd16a3c`.
  Original content, scenario, builds and runtime observations were preserved.
- After accepted-main integration, the same owner completed public target
  refresh and neutral reconnect revalidation under the actual ordered leases.
  All historical request-envelope hashes were checked. The retained executor,
  verifier and older topology stayed stopped during this evidence-only resume.

The original tested build `d98afdaf4fc3`, original runtime build
`4000b7325b12`, and the two advanced tested builds are distinct.
A tested-build result is never assigned to a different runtime build.
The retained advancement is complete through its tested content build; this
record does not claim a new retained topology publication. For future isolated
runtime production, use the accepted [runtime handoff](RUNTIME_HANDOFF.md) and
the exact retained plan fence rather than copying private delivery evidence
into the executor.

## Evidence identifiers and validation

The private evidence is retained by its owners. These SHA-256 identifiers allow
correlation without publishing recovery paths, raw delivery records or secrets.

| Evidence | SHA-256 |
| --- | --- |
| Historical independent portable audit | `f8bf042cb4e2aa9f5a427208ec05119f3eb3ef7135f7a28ae92c44e3b61fb528` |
| October portable input record | `c50b239d71a1d1fbe94f0e04041c951943253ea5be81ee2a6677be811abebc04` |
| October export verification | `c34eec9d37db1ed0e371e9a2926e0939b4848902802e03e7ddcbe45e09665495` |
| October isolated relocation verification | `5fe1262cd025a98fcf9b3b7d85e5995efcac14877e36564983b9cb9942fe16bf` |
| October explicit wrong-pin refusal log | `3b353638f9dc4d6f96676a68b3fe8594ae11a35bb46ff6afe34943ce43fe2087` |
| Final maintainer observation | `43bf478bee6fe9038a0cbdb7e63cffb611a7d02ab6cbcb43f2115bc878b12315` |
| Session-aligned native process observation | `75da03079f3553e08bc3b57ac9011457e052c23b42d02a99e74f6be9d868968e` |
| Successful 307-test integration log | `6054111ccf8e49141e02c9660984e3dfc58df0cd561e98b469771247cfb21798` |

The 307-test run passed with four skips. Final evidence-delivery validation and
independent whole-diff review are recorded in the PR, separately from application
runtime acceptance. Parent #562 remains open until its required deliveries are
merged and its complete acceptance is confirmed.

See [Linux execution](LINUX_EXECUTION.md), [runtime handoff](RUNTIME_HANDOFF.md)
and [source delivery](SOURCE_DELIVERY.md) for the supported operation boundaries.
