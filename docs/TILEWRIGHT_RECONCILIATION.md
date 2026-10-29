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
| [Atrinik content toolkit](https://github.com/atrinik/content-toolkit) | Existing Atrinik-format parsing, catalogs, validation and migration tooling, including the transition to shared tooling |
| [Atrinik content](https://github.com/atrinik/content) | Authored maps, archetypes, quests and lore; `main` remains the sole mutable authored source |
| [Atrinik Classic](https://github.com/atrinik/classic) | Maintained playable C client/server and Classic authoring support |
| [Atrinik server](https://github.com/atrinik/server), [client](https://github.com/atrinik/client) and [protocol](https://github.com/atrinik/protocol) | Atrinik-specific replacement simulation, sessions, game UI, protocol and adapters to shared tooling |
| This workspace | Explicit repository composition and isolated builds, profiles, state, scenarios and runtime supervision |

Game rules, quests, protocol messages, gameplay validation and content licenses
stay with Atrinik. Shared Tilewright packages must not acquire Atrinik-specific
rules to replace an adapter. A game profile describes the game to the shared
tools; it does not transfer authority over gameplay to the editor or renderer.

The former `atrinik/renderer` and `atrinik/editor` repositories were transferred
to the Tilewright repositories above. Their old GitHub URLs redirect to the
same repositories; they are not duplicate repositories awaiting deletion.
Preserve those redirects and their history, issues, releases and attribution.

The workspace currently retains the logical selectors and checkout paths
`renderer` and `editor`. `components.json` still uses the redirected repository
coordinates. Updating those coordinates requires handling existing checkout
remote validation as well as new clones; a documentation update alone must not
make existing checkouts fail repository identity checks.

## Crate transition

Repository names, Rust package names, released versions and import paths are
separate contracts. A repository transfer does not rename a published crate.

The source inventory on 2026-10-29 has these important boundaries:

- Tilewright renderer source uses `tilewright-*` package names.
- Tilewright world source uses its own `tilewright-*` packages and a new world
  format. Its release guide does not currently enable crates.io publication.
- Tilewright editor still consumes the published `atrinik-scene`,
  `atrinik-render-api`, `atrinik-render-resources`, `atrinik-render-testkit`,
  `atrinik-source` and `atrinik-transaction` packages at version `0.1.0`.
- Atrinik content toolkit still owns the `atrinik-source` and
  `atrinik-transaction` implementations used by that editor. Tilewright world's
  parser and transaction APIs must be assessed for compatibility; they are not
  assumed to be package aliases.
- `atrinik-protocol` remains an Atrinik gameplay protocol package. Its prefix
  is not a reason to rename or remove it.

Verify current manifests, registry artifacts and release policy before each
consumer change. Reconcile names through this sequence:

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

The migration order between shared tooling, a new server and a later client
needs its own accepted milestones. In particular, using a new server with the
Classic client requires an explicit compatibility design; it is not implied by
sharing a renderer or world format. Existing functional-equivalence and runtime
cutover issues retain their acceptance responsibilities until reconciled.

## Reconciliation work remaining

- Update the workspace manifest's transferred repository coordinates with
  scoped compatibility for existing checkout remotes. Reconcile active
  governance and dependency inventories while preserving immutable provenance.
- Assign the generic/toolkit boundary and decide whether Atrinik content toolkit
  remains a permanent importer/adapter or retires after its responsibilities
  have moved. Keep its existing consumers supported during that decision.
- Publish and validate the crate transition above before switching editor pins.
- Update active roadmap issues and consumer guides to name the canonical
  Tilewright owners and continued Classic support. Preserve historical evidence
  and deliberate future cutover gates rather than mechanically replacing text.
- Audit proposed retirement candidates for consumers, releases, unresolved
  issues, CI, package publication and historical value. Present exact repository
  identities and a disposition for owner approval before archival or deletion.

No repository has been established as unused by this inventory. In particular,
the content toolkit has an active editor consumer, and the replacement client,
server and protocol still own future Atrinik-specific work. Their incomplete
runtime integration is not evidence that they can be deleted.
