# Integration architecture

## Ownership boundaries

The integration repository owns only the release lock, secure synchronization,
cross-component build orchestration, and system-level documentation. Component
repositories own their source, component tests, release packaging, and local
developer instructions. Changes should be made at the narrowest owning
repository and consumed here only after that repository publishes an immutable
release.

Issue ownership follows the same boundary. A single-component issue belongs in
that component repository; only roadmaps, repository topology, integration
compatibility, and genuinely cross-component coordination remain here.

The shared C library is `atrinik/libatrinik`. The game command contract is
`atrinik/protocol`; it generates both the C interface used by the client and
server and the Python package intended for automation. Content source and
collected server runtime data are two release assets of `atrinik/content`.
Sound and server resources remain independent asset repositories because their
licensing and release cadence differ from executable code.

## Locked assembly

```text
components.lock.json
        |
        v
scripts/components.py -- SHA-256 verification and bounded extraction
        |
        +--> build/components/protocol ------+
        +--> build/components/libatrinik ----+--> client and server builds
        +--> build/components/client --------+
        +--> build/components/server --------+
        +--> build/components/content
        +--> client/sound runtime data
        +--> server/content and resource runtime data
```

Release tags provide human-readable versions, commits identify the exact source
revision, and SHA-256 digests authenticate the selected release bytes. All
three are recorded because none is an adequate substitute for the others.
Git submodules are deliberately excluded: they do not pin generated release
artifacts, complicate shallow and archive-based checkouts, and make asset
licensing boundaries less visible.

The synchronizer installs into ignored directories using a staging directory
and an atomic rename. A marker records the selected repository, tag, commit,
and digest. Existing directories without that marker are never replaced.
Archive members are limited by count, individual size, and total expanded size;
absolute paths, parent traversal, links, devices, duplicate case-folded paths,
and Windows-style paths are rejected. Installation destinations must also be
canonical and unique under case folding, so a lock behaves consistently on
case-sensitive and case-insensitive hosts. Locks accept only release assets
owned by the Atrinik GitHub organization and cap the number of component
entries.

The integration verifier also reads the dependency locks embedded in the
selected client and server releases. Their protocol, library, sound, content,
and resource pins must exactly match this manifest before a build can pass.
Each consumer must declare its complete required set. This prevents a top-level
lock update from silently asserting compatibility that the released consumer
did not declare.

Parent components are installed before destinations nested inside them. If a
parent changes, every nested dependency is forcibly reinstalled from its own
verified archive, even if the parent archive happened to contain a matching
management marker. This keeps the nested release boundary authoritative.

## Release flow

1. Change and validate a component in its owning repository.
2. Merge a pull request whose title follows Conventional Commits style.
3. semantic-release parses the squash commit. A breaking change bumps major,
   `feat` bumps minor, and every other conventional type bumps at least patch.
4. The repository publishes its immutable tag and owned artifacts: source and
   Windows packages for the client; source and Windows packages plus the
   container image for the server; source and wheel packages for protocol;
   source and collected runtime packages for content; archives for sound,
   resources, and the shared library; and both build images for devcontainer.
5. Verify the release asset checksum and resolve the tag to its full commit.
6. Update only the relevant lock entry here.
7. Run component-lock tests and the complete client/server integration build.
8. Merge the integration pull request after its required checks pass.

Integration CI authenticates to GHCR and runs the complete assembly inside the
immutable `atrinik/linux-build:1.0.2` image digest. That image supplies the
native dependencies and pinned SDL mixer build; ccache and release downloads
are persisted separately in the Actions cache. Update the human-readable image
tag and digest together only after the devcontainer release publishes and its
non-root build path has been validated.

Client and server product versions are declared by those repositories. The
integration repository does not invent or infer component versions.

## Runtime data flow

The client consumes sound data released by `atrinik/sound`. The server
consumes collected content released by `atrinik/content` and auxiliary runtime
resources released by `atrinik/resources`. These archives are installed inside
their assembled component trees so the component-provided runtime and test
scripts see the same marker contract used by standalone checkouts.

Authored content remains separate from collected runtime output. Never edit a
collected archive in the integration tree; change its source in
`atrinik/content`, rebuild it deterministically, and update the lock.

## Protocol flow

`atrinik/protocol` is the sole source for command IDs and generated language
bindings. The client, server, shared library, and future Python tooling pin its
release rather than vendoring a snapshot. A coordinated protocol change must
update all current producers and consumers, publish the protocol first, then
publish dependent components and update this integration lock.
