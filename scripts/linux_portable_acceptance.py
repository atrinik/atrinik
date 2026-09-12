#!/usr/bin/env python3
"""Nonpublishing portable CI acceptance; never grants coordinator authority."""
from __future__ import annotations

import argparse
import ctypes
import hashlib
import json
import os
from pathlib import Path
import subprocess
import sys

sys.path.insert(0, str(Path(__file__).resolve().parent.parent))
from atrinik_workspace import linux_export, linux_portable

SOURCE_COMMITS = {
    "classic": linux_portable.CONSUMER_COMMIT,
    "sound": "d0561bf9ff8dc88836818dbe602a5a256c6c0e3f",
}
SOUND_RELEASE = {
    "repository": "atrinik/sound", "tag": "v1.0.0",
    "product": "atrinik-sound-classic-runtime", "product_version": "1.0.0",
    "manifest_schema_version": 1,
    "source_commit": SOURCE_COMMITS["sound"],
    "source_tree": "f464de12f943f1844f8587ca1a7419f6b78b9e44",
    "asset_url": "https://github.com/atrinik/sound/releases/download/v1.0.0/atrinik-sound-classic-runtime-1.0.0.tar.gz",
    "archive_sha256": "e3f17d314b3933db9c6af3f9290c5df79375a6cc3d570ea29f225b53de362784",
    "release_manifest_sha256": "2d7a1ba78e4f484aa0554809f72b14345c40d37cc2d46daf0416de69627f2cbb",
    "source_manifest_sha256": "3aacd122abe16da771ac1eb6ad80c50c1c6e7ab43d555dc8772f21be24248366",
    "schema_sha256": "428e1312d9922ab4ec20c0ee89d93d842528db6d8cc75197c135f4d4f59066aa",
    "toolchain_sha256": "ee842444c37df3c6784665c2dacef4ab9220f3abfc5c2daf9214fe4b40aadbf7",
    "output_tree_sha256": "2c3d42ea91ca088ac37e5215e3452dad31e5f1fb17018941ed5ad0a3c53060da",
}


def run(arguments, *, timeout=3600, env=None):
    result = subprocess.run([str(value) for value in arguments], text=True,
                            stdout=subprocess.PIPE, stderr=None,
                            timeout=timeout, check=True, env=env)
    return result.stdout


def save(path, value):
    path.write_text(json.dumps(value, indent=2, sort_keys=True) + "\n")


def require_headless():
    if os.geteuid() == 0:
        raise RuntimeError("acceptance must run as a non-root user")
    if any(os.environ.get(name) for name in
           ("DISPLAY", "WAYLAND_DISPLAY", "PULSE_SERVER", "PIPEWIRE_REMOTE")):
        raise RuntimeError("CPU acceptance must not receive display or audio endpoints")
    if Path("/dev/dri").exists() or any(Path("/dev").glob("nvidia*")):
        raise RuntimeError("CPU acceptance must not receive GPU devices")


def build(output, evidence):
    require_headless()
    os.sched_setaffinity(0, sorted(os.sched_getaffinity(0))[:2])
    linux_portable.installed_metadata()
    wrapper_head = run(["git", "rev-parse", "HEAD"]).strip()
    if run(["git", "status", "--porcelain"]):
        raise RuntimeError("acceptance requires a clean wrapper checkout")
    run(["./atrinik", "init", "classic-client", "sound", "--jobs", "2"])
    # The producer deliberately fails on source drift: update the qualified
    # producer and its checked-in recipe together instead of relabeling bytes.
    for name, expected in SOURCE_COMMITS.items():
        if run(["git", "-C", name, "rev-parse", "HEAD"]).strip() != expected:
            raise RuntimeError("qualified source changed: " + name)
        if run(["git", "-C", name, "status", "--porcelain"]):
            raise RuntimeError("dirty dependency: " + name)
    profile = "linux-portable-acceptance"
    run(["./atrinik", "profile", "create", profile, "--from", "classic"])
    arguments = ["./atrinik", "profile", "sound-mode", profile, "released"]
    flags = {
        "repository": "repository", "tag": "tag", "product_version": "product-version",
        "source_commit": "source-commit", "source_tree": "source-tree", "asset_url": "asset-url",
        "archive_sha256": "archive-sha256", "release_manifest_sha256": "manifest-sha256",
        "source_manifest_sha256": "source-manifest-sha256", "schema_sha256": "schema-sha256",
        "toolchain_sha256": "toolchain-sha256", "output_tree_sha256": "tree-sha256",
    }
    for key, flag in flags.items():
        arguments.extend(["--release-" + flag, SOUND_RELEASE[key]])
    run(arguments)
    result = json.loads(run(["./atrinik", "linux", "export", "--profile", profile,
                             "--output", output]))
    save(evidence / "export.json", result)
    save(evidence / "inputs.json", {"wrapper_head": wrapper_head,
         "sources": SOURCE_COMMITS, "sound": SOUND_RELEASE,
         "image": linux_portable.IMAGE, "platform_manifest": linux_portable.PLATFORM_MANIFEST})
    # Keep the executable's exact configure/compiler evidence without copying
    # source/build trees into the isolated runtime test environment.
    for cache in Path("workspace/build/profiles").glob(profile + "-*/build/client/CMakeCache.txt"):
        (evidence / "CMakeCache.txt").write_bytes(cache.read_bytes())
        commands = cache.parent / "compile_commands.json"
        if commands.is_file():
            (evidence / "compile_commands.json").write_bytes(commands.read_bytes())


def bind_function(library, name, result, arguments):
    function = getattr(library, name)
    function.restype = result
    function.argtypes = arguments
    return function


def loaded_application_paths(root):
    paths = set()
    application_names = {path.name for path in (root / "lib").rglob("*") if path.is_file()}
    for line in Path("/proc/self/maps").read_text().splitlines():
        value = line.split(maxsplit=5)
        if len(value) == 6 and value[5].startswith("/"):
            path = Path(value[5])
            if path.name in linux_portable.HOST_GLIBC:
                continue
            if path.name in application_names and not path.is_relative_to(root):
                raise RuntimeError("application provider escaped moved export: " + str(path))
            if path.is_relative_to(root):
                paths.add(str(path.relative_to(root)))
    return sorted(paths)


def verify(output, evidence):
    require_headless()
    if Path("/workspaces/atrinik").exists() or Path("/opt/atrinik-portable").exists():
        raise RuntimeError("original source/build and producer prefixes must be unavailable")
    result = linux_export.verify_export(output)
    portable = json.loads((output / "portable-evidence.json").read_text())
    if portable["runtime"]["source_commit"] != linux_portable.CONSUMER_COMMIT:
        raise RuntimeError("unexpected portable consumer identity")
    library_root = output / "lib"
    libraries = {}
    for path in sorted(library_root.iterdir()):
        if path.is_file():
            libraries[path.name] = ctypes.CDLL(str(path), mode=os.RTLD_NOW | os.RTLD_GLOBAL)
    sdl = libraries["libSDL3.so.0"]
    error = bind_function(sdl, "SDL_GetError", ctypes.c_char_p, [])
    image = libraries["libSDL3_image.so.0"]
    load_image = bind_function(image, "IMG_Load", ctypes.c_void_p, [ctypes.c_char_p])
    destroy_image = bind_function(sdl, "SDL_DestroySurface", None, [ctypes.c_void_p])
    font = libraries["libSDL3_ttf.so.0"]
    if not bind_function(font, "TTF_Init", ctypes.c_bool, [])():
        raise RuntimeError("TTF_Init: " + str(error()))
    open_font = bind_function(font, "TTF_OpenFont", ctypes.c_void_p, [ctypes.c_char_p, ctypes.c_float])
    close_font = bind_function(font, "TTF_CloseFont", None, [ctypes.c_void_p])
    media = output / "share/games/atrinik"
    images = fonts = 0
    for path in sorted(media.rglob("*")):
        if path.suffix.lower() in {".png", ".jpg", ".jpeg"}:
            surface = load_image(os.fsencode(path))
            if not surface:
                raise RuntimeError("actual image decode failed: " + str(path) + ": " + str(error()))
            destroy_image(surface)
            images += 1
        elif path.suffix.lower() in {".ttf", ".otf"}:
            handle = open_font(os.fsencode(path), 14.0)
            if not handle:
                raise RuntimeError("actual font load failed: " + str(path) + ": " + str(error()))
            close_font(handle)
            fonts += 1
    if not images or not fonts:
        raise RuntimeError("export lacks actual image/font media")
    mixer = libraries["libSDL3_mixer.so.0"]
    if not bind_function(mixer, "MIX_Init", ctypes.c_bool, [])():
        raise RuntimeError("MIX_Init: " + str(error()))
    decoder = bind_function(mixer, "MIX_CreateAudioDecoder", ctypes.c_void_p,
                            [ctypes.c_char_p, ctypes.c_uint32])
    decode = bind_function(mixer, "MIX_DecodeAudio", ctypes.c_int,
                           [ctypes.c_void_p, ctypes.c_void_p, ctypes.c_int, ctypes.c_void_p])
    destroy = bind_function(mixer, "MIX_DestroyAudioDecoder", None, [ctypes.c_void_p])
    class AudioSpec(ctypes.Structure):
        _fields_ = [("format", ctypes.c_uint32), ("channels", ctypes.c_int), ("freq", ctypes.c_int)]
    get_format = bind_function(mixer, "MIX_GetAudioDecoderFormat", ctypes.c_bool,
                               [ctypes.c_void_p, ctypes.POINTER(AudioSpec)])
    sound = media / "sound"
    manifest = json.loads((sound / "classic-runtime-manifest.json").read_text())
    if len(manifest["assets"]) != 339:
        raise RuntimeError("released media closure changed")
    decoded = []
    buffer = ctypes.create_string_buffer(64 * 1024)
    for row in manifest["assets"]:
        path = sound / row["logical_path"]
        handle = decoder(os.fsencode(path), 0)
        if not handle:
            raise RuntimeError("actual audio decoder rejected " + row["logical_path"] + ": " + str(error()))
        try:
            spec = AudioSpec()
            if not get_format(handle, ctypes.byref(spec)):
                raise RuntimeError("audio format query failed: " + row["logical_path"])
            count = decode(handle, buffer, len(buffer), ctypes.byref(spec))
            if count <= 0:
                raise RuntimeError("actual audio decode failed: " + row["logical_path"] + ": " + str(error()))
            decoded.append({"path": row["logical_path"], "pcm_prefix_bytes": count,
                            "pcm_prefix_sha256": hashlib.sha256(buffer.raw[:count]).hexdigest()})
        finally:
            destroy(handle)
    crypto = libraries["libcrypto.so.3"]
    provider_load = bind_function(crypto, "OSSL_PROVIDER_load", ctypes.c_void_p,
                                 [ctypes.c_void_p, ctypes.c_char_p])
    providers = [provider_load(None, name) for name in (b"default", b"legacy")]
    if not all(providers):
        raise RuntimeError("relocated OpenSSL provider closure failed")
    output_text = run([output / "atrinik", "--help"], timeout=30)
    if not output_text.strip():
        raise RuntimeError("relocated executable did not produce command help")
    save(evidence / "relocation.json", {"inventory": result, "images_decoded": images,
         "fonts_loaded": fonts, "audio_decoded": decoded,
         "application_mappings": loaded_application_paths(output),
         "openssl_providers": ["default", "legacy"], "executable_help": True,
         "original_source_build_paths_available": False,
         "hardware_gameplay_verified": False, "audible_playback_verified": False})


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("mode", choices=("build", "verify"))
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--evidence", type=Path, required=True)
    args = parser.parse_args()
    args.evidence.mkdir(parents=True, exist_ok=True)
    (build if args.mode == "build" else verify)(args.output, args.evidence)


if __name__ == "__main__":
    main()
