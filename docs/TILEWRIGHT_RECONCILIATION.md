# Atrinik and Tilewright ownership

Atrinik continues to develop and support its playable Classic implementation
while preparing a later migration to shared tooling and replacement runtime
components. Replacement work does not put Classic fixes or approved development
on hold. A new server or client becomes the supported runtime only after its
compatibility, gameplay, packaging, operations and rollback acceptance is met.

## Repository responsibilities

| Owner | Responsibility |
| --- | --- |
| [Tilewright renderer](https://github.com/seraphinastudios/tilewright-renderer) | Game-neutral scene types, GPU rendering, presentation bridges and offscreen rendering |
| [Tilewright world](https://github.com/seraphinastudios/tilewright-world) | Game-neutral world format, validation, compilation, pack readers and editing transactions |
| [Tilewright editor](https://github.com/seraphinastudios/tilewright-editor) | Shared desktop/headless editor and agent tooling, configured through game profiles |
| [Atrinik content toolkit](https://github.com/atrinik/content-toolkit) | Active Atrinik importer/exporter, game-format parsing, catalogs, validation and Classic compatibility adapters |
| [Atrinik content](https://github.com/atrinik/content) | Authored maps, archetypes, quests and lore; `main` remains the sole mutable authored source |
| [Atrinik Classic](https://github.com/atrinik/classic) | Maintained playable C client/server and Classic authoring support |
| [Atrinik server](https://github.com/atrinik/server), [client](https://github.com/atrinik/client) and [protocol](https://github.com/atrinik/protocol) | Atrinik-specific replacement simulation, sessions, game UI, protocol and adapters to shared tooling |
| This workspace | Explicit repository composition and isolated builds, profiles, state, scenarios and runtime supervision |

Game rules, quests, protocol messages, gameplay validation and content licenses
stay with Atrinik. Shared Tilewright packages must not acquire Atrinik-specific
rules to replace an adapter. A game profile describes the game to the shared
tools; it does not transfer authority over gameplay to the editor or renderer.

The owner resolved the toolkit boundary on 2026-10-30: retain
`atrinik/content-toolkit` as the Atrinik importer/exporter and Classic
compatibility layer, and move shared, game-neutral functionality into
Tilewright. Atrinik rules, formats and game-profile adapters remain with
Atrinik. This is an ownership decision, not proof of content import/export
parity or authority to change canonical formats or runtime inputs.

The former `atrinik/renderer` and `atrinik/editor` repositories were transferred
to the Tilewright repositories above. Their old GitHub URLs redirect to the
same repositories; they are not duplicate repositories awaiting deletion.
Preserve those redirects and their history, issues, releases and attribution.

The workspace retains the logical selectors and checkout paths `renderer` and
`editor`. `components.json` uses the canonical Tilewright repositories for new
clones. Offline checkout and supply-chain validation also accept each one's
exact historical Atrinik coordinate, without rewriting local remotes or making
network requests. This fixed compatibility policy assumes the old paths remain
redirects to the same repositories: do not recreate them for different projects.
It neither discovers arbitrary redirects nor proves live repository identity.
Foreign hosts, forks and editor/renderer coordinate swaps remain invalid.

## Crate transition

Repository names, Rust package names, released versions and import paths are
separate contracts. A repository transfer does not rename a published crate.

### Historical inventory: 2026-10-29

The initial source inventory found Tilewright renderer and world source using
`tilewright-*` names, while the editor still consumed published
`atrinik-scene`, `atrinik-render-api`, `atrinik-render-resources`,
`atrinik-render-testkit`, `atrinik-source` and `atrinik-transaction` at `0.1.0`.
The world's release guide did not then enable crates.io publication. The
content toolkit owned the editor's source/transaction implementations; the
world parser and transaction APIs required compatibility assessment rather
than being assumed to be package aliases. These are dated findings, superseded
by the released-package migration below.

### Completed publication and editor migration: 2026-10-30

All thirteen successor crates are published and verified in the public
registry, including exact API/index versions, archive hashes and bytes,
package ownership and non-yanked status.

| Producer release | Immutable source revision | Published crates |
| --- | --- | --- |
| [Renderer v1.18.0](https://github.com/seraphinastudios/tilewright-renderer/releases/tag/v1.18.0) (release `428070421`) | `a061c6e66c6166d66c757745eb8a91b716bc9ab5` | `tilewright-scene`, `tilewright-render-resources`, `tilewright-render-ui`, `tilewright-render-world`, `tilewright-render-api`, `tilewright-render-testkit`, `tilewright-render-wgpu`, `tilewright-render-sdl3`, all `0.5.1` |
| [World v0.1.0](https://github.com/seraphinastudios/tilewright-world/releases/tag/v0.1.0) (release `428127917`) | `70a86f25f9f6c3077991309a65137afb7a08a54d` | `tilewright-jsonc`, `tilewright-source`, `tilewright-pack`, `tilewright-diff`, `tilewright-edit`, all `0.1.0` |

[Editor PR #42](https://github.com/seraphinastudios/tilewright-editor/pull/42)
merged on 2026-10-30 as
`9f872b1b447ebf08924f1130bd548c5178dfb84a`. The registry-backed `Cargo.lock`
now consumes World `edit`, `jsonc` and `source` at `0.1.0`, and renderer `api`,
`resources`, `testkit`, `world` and `scene` at `0.5.1`, using the full package
names above. It no longer relies on the toolkit's source/transaction crates.

The editor's complete registry-based `tools/validate.sh` passed 28 Rust tests
plus doc tests, seven Python provenance tests, license/security checks, smokes,
SBOM/provenance checks and reproducible packages. Hosted Linux and Windows
validation and packaging also
[passed before merge](https://github.com/seraphinastudios/tilewright-editor/actions/runs/41128457741)
and [on the merged main commit](https://github.com/seraphinastudios/tilewright-editor/actions/runs/41130211936).
Editing, undo and redo use World transaction APIs. `RecoveryStore` remains an
application integration interface; this migration supplies no new production
recovery backend, GUI or CLI. Generic editor migration does not establish
functional parity for Atrinik game content, Classic import/export compatibility,
a canonical format change or a runtime cutover.

`atrinik-protocol` remains an Atrinik gameplay protocol package. Its prefix is
not a reason to rename or remove it.

Verify current manifests, registry artifacts and release policy before each
additional consumer change. The completed editor transition does not waive
this sequence for other consumers:

1. Inventory each package's producer, exact consumer pins, release provenance,
   public API and ownership. Distinguish a historical coordinate from a live
   dependency. Record the intended Tilewright successor where one exists.
2. Provide the required Tilewright APIs and immutable released artifacts under
   the producer's release policy. Record exact versions, checksums and source
   revisions. Confirm package ownership and publication readiness before
   changing consumers; do not substitute an unreleased name or sibling path.
3. Migrate each consumer's manifests, imports, lockfiles, dependency-update
   policy, release verification and documentation together. Use adapters where
   the world model or transaction API changes. Validate against released
   packages rather than assuming a successful local source build is sufficient.
4. Exercise editor document loading, validation, preview, save/undo and renderer
   integration with representative Atrinik content. Prove the import/export
   boundary before changing any canonical authored format or Classic build input.
5. Retire superseded implementation only after its consumers and outstanding
   acceptance responsibilities have moved. Preserve historical release
   coordinates, attribution and provenance. Package yanking, repository
   archival and repository deletion are separate owner decisions.

## Continued Classic support and later migration

Classic remains an active development and release target. Continue bug fixes,
performance work, supported packaging and approved gameplay/content changes
while shared tooling and replacement components progress. Older roadmap text
describing Classic as critical-maintenance-only must be reconciled with this
policy; it must not be used to refuse owner-authorized Classic work.

Both stacks continue to consume compatible products derived from
`atrinik/content@main`. Keep Classic and replacement protocols, provider
selection and mutable state isolated. Do not silently route a missing
replacement adapter through Classic or open Classic player state with the new
server. Tooling reconciliation does not establish protocol or save compatibility.

The first migration milestone is shared editor, world and content tooling.
Server and client migration follow later; this priority does not hold up Classic
development or fixes. Using a new server with the Classic client would require
an explicit compatibility design; it is not implied by sharing a renderer or
world format. Existing functional-equivalence and runtime cutover issues retain
their acceptance responsibilities until reconciled.

## Reconciliation work remaining

- Keep active governance and dependency inventories aligned with the canonical
  transferred repository coordinates while preserving immutable provenance.
- Apply the chosen toolkit boundary: move shared, game-neutral functionality
  to Tilewright while retaining Atrinik import/export, game formats, catalogs
  and Classic compatibility in the active Atrinik toolkit. Keep existing
  consumers supported.
- Prove representative Atrinik content loading, validation, preview, save/undo
  and import/export compatibility before changing canonical authored formats
  or Classic build inputs. The completed registry/editor migration does not
  satisfy this content acceptance.
- Update active roadmap issues and consumer guides to name the canonical
  Tilewright owners and continued Classic support. Preserve historical evidence
  and deliberate future cutover gates rather than mechanically replacing text.
- Audit proposed retirement candidates for consumers, releases, unresolved
  issues, CI, package publication and historical value. Present exact repository
  identities and a disposition for owner approval before archival or deletion.

No repository has been established as unused by this inventory. In particular,
the content toolkit retains the explicitly chosen Atrinik import/export and
Classic compatibility responsibilities despite the generic editor dependency
migration. Classic, content, tools and the replacement client, server and
protocol remain active owners. Incomplete runtime integration is not evidence
that any of them can be deleted. Preserve repository history and redirects;
archival requires an exact candidate audit and a separate owner decision.
