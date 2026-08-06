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

## Legacy QUIC gameplay and asset transport

`common/toolkit/socket_quic.c` owns the certificate-pinned OpenSSL QUIC
connection and `common/toolkit/socket.c` owns explicit application-stream
lifecycle. The `atrinik/2` ALPN disables OpenSSL's default stream. The client
opens one typed bidirectional game stream; existing `socket_read()` and
`socket_write()` target only that stream. Explicit asset stream helpers are
used by `client/src/client/asset.c` and `server/src/socket/assets.c`. Each asset
stream carries one request and one immutable response, so bulk bytes never
enter the game stream or `server/src/socket/lowlevel.c`'s packet FIFO.

The client's single transport thread owns the connection and every client
stream. It drains game output/input before servicing at most three active asset
streams in bounded round-robin quanta. The server game-loop networking path is
the corresponding sole owner: it processes and flushes game traffic before
accepting or advancing asset streams. Server asset states retain only a
snapshot entry reference and cursor; `server/src/socket/assets.c` owns the
allowlist, immutable 1 GiB snapshot, 128 MiB object ceiling, request abuse
limit, and per-connection token bucket. `common/toolkit/socket_asset.c` owns
the request and fixed response-header encoding. `doc/ADS/ADS-2` is the
authoritative byte-level contract.

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
