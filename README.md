# Atrinik integration

This repository assembles a tested Atrinik system from independently released
components. It intentionally contains no client, server, shared-library,
protocol, content, or media source code and uses no Git submodules.

`components.lock.json` is the source of truth for an integration snapshot. Each
entry identifies an immutable release tag and commit, the exact release asset,
its SHA-256 digest, and its generated destination under `build/components/`.

## Repository map

| Repository | Ownership |
| --- | --- |
| [`atrinik/client`](https://github.com/atrinik/client) | SDL game client and client packaging |
| [`atrinik/server`](https://github.com/atrinik/server) | Game server, plugins, runtime assembly, and server packaging |
| [`atrinik/libatrinik`](https://github.com/atrinik/libatrinik) | Shared C networking and utility library |
| [`atrinik/protocol`](https://github.com/atrinik/protocol) | Canonical game command schema and generated C/Python bindings |
| [`atrinik/content`](https://github.com/atrinik/content) | Maps, archetypes, scripts, source assets, and collected runtime content |
| [`atrinik/sound`](https://github.com/atrinik/sound) | Client music and sound assets |
| [`atrinik/resources`](https://github.com/atrinik/resources) | Server runtime resources |
| [`atrinik/tools`](https://github.com/atrinik/tools) | Standalone authoring, inspection, and diagnostic tools |
| [`atrinik/editor`](https://github.com/atrinik/editor) | Gridarta editor packaging |
| [`atrinik/metaserver-worker`](https://github.com/atrinik/metaserver-worker) | Metaserver service |

Future Python automation and bot packages belong in `atrinik/tools` and should
consume a pinned `atrinik-protocol` wheel instead of copying command IDs or
packet definitions.

## Issues and project coordination

File component-owned work in its owning repository: client UI/rendering in
`atrinik/client`, simulation/runtime work in `atrinik/server`, authored content
and content tooling in `atrinik/content`, wire contracts in
`atrinik/protocol`, shared C APIs in `atrinik/libatrinik`, and standalone tools
in `atrinik/tools`.

This integration repository retains project roadmaps, repository-boundary
work, coordinated changes that genuinely span multiple components, website
coordination until that repository exists, and compatibility/release-manifest
issues. It also preserves the original project history and stable links.
Component-specific issues have been transferred to their owners; GitHub keeps
redirects and rewrites cross-repository parent/prerequisite references.
Matching M1–M5 roadmap milestones live in each repository that owns work for
that phase, while the cross-phase master roadmap remains here as the index.

## Assemble and validate

Python 3.11 or newer is required to synchronize components:

```sh
python3 scripts/components.py validate
python3 scripts/components.py sync
python3 scripts/components.py verify
```

The synchronizer downloads only the locked GitHub release assets, verifies
their digests before extraction, rejects links and unsafe archive paths, and
refuses to overwrite unmanaged destinations. Verification also rejects a
client or server release whose own protocol, library, sound, content, or
resource lock differs from the integration snapshot. Generated files remain
below `build/` and are ignored by Git.

After installing the client and server build prerequisites documented in their
repositories, build and test the complete source snapshot with:

```sh
scripts/build.sh
```

The script supplies the locked local protocol and library sources to both
components through standard CMake `FetchContent` overrides, runs both CTest
suites, prepares the server runtime from the pinned content/resources, and
executes the server's non-listening version smoke check. See
[`docs/ARCHITECTURE.md`](docs/ARCHITECTURE.md) for release and data-flow
details. [`docs/DEVELOPMENT.md`](docs/DEVELOPMENT.md) covers editable component
worktrees, devcontainer launch commands, and safely sharing one mutable server
data directory across server worktrees.

## Releases

Every squash merge in each active Atrinik repository uses its Conventional
Commits pull-request title as the semantic-release input. Breaking changes bump
major, `feat` bumps minor, and every other conventional type bumps at least
patch, so every merged pull request receives an immutable release tag. New
repositories start at `v1.0.0`; no active release line remains at `0.0.x`.

Release jobs build the artifacts owned by their repository. In particular,
every client release publishes a Windows x86_64 package, every server release
publishes a Windows x86_64 package and server container, protocol releases
publish a Python wheel, content releases publish source and collected runtime
archives, and both devcontainer build images are published for every
devcontainer release.

## Licensing

The integration scripts and documentation in this repository are MIT licensed.
Each synchronized component retains the license and attribution published in
its own release archive.
