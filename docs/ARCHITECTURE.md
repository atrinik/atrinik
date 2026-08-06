# Atrinik architecture index

This document identifies the source-of-truth boundaries and important data
flows that span Atrinik components. Component README files remain the detailed
operating guides.

## Legacy client rendering and platform layer

The legacy client under `client/` targets SDL 3 directly. Its rendering model
is deliberately CPU-based:

1. `client/src/client/video.c` owns the `SDL_Window` and obtains its current
   `SDL_Surface` with `SDL_GetWindowSurface()`.
2. Client, map, widget, popup, font, lighting, and sprite code compose each
   frame into that software surface.
3. `client/src/client/main.c` presents the completed frame with
   `SDL_UpdateWindowSurface()`.
4. Window pixel-size and fullscreen changes reacquire the window surface;
   callers must not retain a window-surface pointer across those changes.

`client/src/client/main.c` owns the process-wide `ScreenWindow` and
`ScreenSurface` handles. `client/src/events/event.c` translates SDL 3 window,
keyboard, text-input, text-editing, pointer, and wheel events into the legacy
client's input model. UTF-8 text is preserved through the text-input and font
paths; keyboard shortcuts continue to use SDL keycodes and scancodes.

`client/src/gui/toolkit/surface_primitives.c` is the in-tree source of the
small drawing and rotation API used by the client. It replaces the former
bundled SDL_gfx/rotozoom implementation rather than exposing a general graphics
library. New primitives should only be added for demonstrated client callers.
Window surfaces are commonly XRGB and cannot preserve per-pixel alpha.
Alpha-bearing textures, text, and sprite effects therefore use
`surface_to_display_alpha()` to normalize to `SDL_PIXELFORMAT_RGBA32`; only
known-opaque surfaces use the window-native `surface_to_display()` path. The
client uses SDL's clipboard and window APIs directly and has no X11-specific
platform layer.

Audio is owned by `client/src/client/sound.c` through SDL3_mixer. Sound effects
and music are required on every supported platform; client configuration fails
when SDL3_mixer is unavailable. Windows packages bundle the SDL3 family of
runtime DLLs. The dependency policy and local build commands are documented in
`INSTALL` and `client/README`; `client/CMakeLists.txt` is the authoritative
dependency and packaging definition.

## Build images

The Dockerfiles that produce Atrinik's Linux and Windows build images live in
the separate `atrinik/devcontainer` repository. This repository consumes
immutable published tags from the VS Code devcontainer, Linux CI, Windows
Compose, and release workflows. All consumer pins must be upgraded together
after the matching images are published.

## Generated and runtime boundaries

- `build/` contains generated build and local review output and is not source.
- `server/lib/` contains collected server resources produced from `arch/` and
  related authored inputs by `tools/collect.py`.
- `server/data/` is mutable source-tree runtime state initialized from
  `server/install_data/`.
- `server/resources/` and `client/sound/` are separately versioned submodules.

See the root `README.md` and `INSTALL`, `client/README`, and `server/README` for
component-specific setup and validation procedures.

## Authored content identities

`tools/content_catalog/` is the shared identity and cross-reference layer for
authored gameplay content. It reads authoritative sources directly; it is not
another source of truth. Run it from the repository root with:

```sh
python3 -m tools.content_catalog validate --root .
python3 -m tools.content_catalog emit --root . \
    --output build/content-catalog.json
```

The emitted JSON is deterministic, contains repository-relative source
locations, and belongs under `build/`. Collection runs the same validator
before writing any aggregate file. CTest exposes `content-catalog-unit` and
`content-catalog-validate`; the server `check` label includes both.

Stable IDs are domain-qualified at persistence and interchange boundaries,
for example `quest:lost_memories`, `quest-part:lost_memories::helping_out`,
`region:incuna`, `map:/shattered_islands/world_1_80`, and
`archetype:skill_literacy`. A matching string in another domain never satisfies
a typed reference.

| Domain | Canonical stable key | Authoritative source |
| --- | --- | --- |
| `archetype` | Primary `Object` key; multipart continuation objects are not independently addressable | Authored `arch/**/*.arc` files |
| `artifact` | `artifact` key | Authored `arch/**/*.art` and `maps/**/*.art` files |
| `treasure` | `treasure` or `treasureone` key; this is also the stable key for a reward backed by a treasure table | Authored `arch/**/*.trs` and `maps/**/*.trs` files |
| `map` | Leading-slash path relative to `maps/`, without rewriting the filename | Authored map file |
| `region` | `region` key | `maps/regions.reg` |
| `faction` | `faction` key | Authored `maps/**/*.factions` files |
| `quest` | Directory name immediately below `maps/interfaces/quests/` | `maps/interfaces/quests/<quest>/` |
| `quest-part` | Quest key plus nested part UIDs joined with `::` | The quest XML's `part uid` attributes |
| `spell` | Spell archetype key, conventionally `spell_<name>` | Type-29 spell archetype in `arch/`; the matching `spellist.h` ID is its runtime mapping |
| `skill` | Skill archetype key, conventionally `skill_<name>` | Type-43 skill archetype in `arch/`; the matching `skillist.h` ID is its runtime mapping |

Display names, messages, descriptions, translations, filesystem enumeration
order, C enum values, and array positions are not identities. In particular,
quest-part UIDs are validated and preserved verbatim; the interface compiler
must never sanitize one into a different key. Runtime spell and skill indices
are process-local acceleration values. New durable data must serialize the
stable archetype key and resolve it to an index after startup.
The object save format writes these as `spell_id` and `skill_id`; the generic
numeric `sp` field remains available for unrelated object types. Reserved skill
table slots without an obtainable skill archetype are internal implementation
details and are not catalog definitions.

Existing monster variants and bosses retain their archetype identities; do not
invent parallel variant or boss IDs for them. A monster-family key does not yet
have an authoritative authored source. The same is true for disciplines,
techniques, activities, achievements, and named landmark records. The feature
that introduces one of those concepts must add its explicit key to its owning
authored schema and teach the catalog loader about that schema in the same
change. A monster's mutable `name` or broad legacy `race` value is not a safe
substitute. Alchemical formulae likewise have no current authored entries or
stable recipe key; the first recipe work must add an explicit recipe key rather
than deriving one from its result title or ingredient order.

### Rename, removal, and migration policy

Before any ID has a durable consumer, a rename may be made atomically by
updating its authoritative definition and every in-repository reference. Once
an ID has been persisted or exchanged externally, its rename or removal must
include a reviewed migration or tombstone owned beside that domain's source.
The migration must identify the old domain-qualified key, the replacement (or
explicit removal), and every durable store that applies it. Tests must cover
both resolution and repeated application.

Do not add aliases or migrations for hypothetical data. Generated catalogs and
runtime lookup tables are rebuilt from the post-migration sources and are
never edited by hand. A migration may be deleted only when every supported
store has recorded its application and no accepted input can still contain the
old key.
