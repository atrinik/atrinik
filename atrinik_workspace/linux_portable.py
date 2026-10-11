"""Produce a movable Classic client from leased sources and the pinned toolchain.

The portable image is a build input, never delivery-coordinator authority.
Publication verifies payload bytes and source/build closure before one no-replace
rename. Interrupted private staging remains available for diagnosis.
"""
from __future__ import annotations

from contextlib import ExitStack
import hashlib
import io
import json
import os
from pathlib import Path
import re
import secrets
import stat
import subprocess
import shlex
import tarfile
from typing import Callable

from . import linux_export as export

IMAGE = "ghcr.io/atrinik/classic-portable-build@sha256:8f6d345f0e24afad5e53d9e35c37f3e3b4012e5cc2c399334711e55e61a2a338"
PLATFORM_MANIFEST = "sha256:7b7f36fef585c5424a56edbe2939a25ab1facbb3dd17a2f3a55980866dd85486"
PRODUCER_COMMIT = "960b41ced51418892f18fc157373d89444c843db"
CONSUMER_COMMIT = "d926f6fd0418fb1af9060158c43d8d3ff5252580"
METADATA_HASHES = {
    "contract.json": "865a9d7fc32fae8a73b2ee3d9c1b30be68d2366dd1aed3257ec34a022d07820b",
    "debian-sources.json": "fd83eb65839b76f692b26cb8b333c9baa0f6ce7379e3d6477351ea501342aec4",
    "installed.json": "85b428e078868940da39a7fa7c7e40de386ca33fcb613e3b1f5bd52f1442f435",
    "runtime-abi.json": "e5bf6972695f1d93779eb964bae73f6015028a4fa8d29ed8c6f180d217a06a13",
    "runtime-sources.json": "ac034c398016886d90d1f8d9601a61040bd760b9c8965c2f258818bf7fa3bf3d",
    "shader-generation.json": "f8e18f2a0e4f1357c7bd415d0b5079f056a609eef7c2453cfaae9884ed5e1563",
}
# Complete final OCI overlay, independently reconstructed from every layer of
# PLATFORM_MANIFEST. Canonical JSON is sorted with compact separators; source
# records contain relative path -> sha256/size/executable. This pins omitted
# archives/recipes/notices as well as bytes, without a caller-supplied override.
PRODUCER_FILES_SHA256 = '838e181c5adef32eea3d82478581d72408012ab27e871af77a2bf93dab934729'
DEBIAN_NOTICES_SHA256 = '6670f1afe9d1face038ae959386c3f264a4b3f05328c493579ea359a8cd2f4e5'
COMMON_LICENSES_SHA256 = 'cfe52936825faa1cd66a4b17ed41051f5c777c5035c8206eaad4db5b730f2bfd'
COMMON_LICENSE_NAMES = ('Apache-2.0', 'Artistic', 'BSD', 'CC0-1.0', 'GFDL-1.2', 'GFDL-1.3', 'GPL-1', 'GPL-2', 'GPL-3', 'LGPL-2', 'LGPL-2.1', 'LGPL-3', 'MPL-1.1', 'MPL-2.0')
# Exact notice bytes in the already pinned source archives. Full embedded
# decoder headers preserve their original license blocks and attribution.
AUDIO_NOTICES = {'libogg': {'members': {'libsdl-org-ogg-936fdd8/AUTHORS': {'sha256': '6fcb33d0cdd60b5054e7df820916388b590427514288c2bab01c626bb8ae9506',
                                                           'size': 198},
                        'libsdl-org-ogg-936fdd8/COPYING': {'sha256': 'd2ab5758336489da61c12cc5bb757da5339c4ae9001f9bb0562b4370249af814',
                                                           'size': 1466}},
            'sha256': '4e2e1da271f4b7b9902f325a714e07d6412e9df7a0dbe43ca53257901c5d74ff'},
 'libopus': {'members': {'libsdl-org-opus-ac9f053/AUTHORS': {'sha256': 'f9fd849e652219ce98e008303370b352d37663d0ffb27b8dc514ad93c9c7438b',
                                                             'size': 239},
                         'libsdl-org-opus-ac9f053/COPYING': {'sha256': '8338ce8d922bb4416ce3dd1e5680173332435e3f0755007ac7801ccd674fe682',
                                                             'size': 1928},
                         'libsdl-org-opus-ac9f053/LICENSE_PLEASE_READ.txt': {'sha256': '7efb4989e0cd1b256229bdf2f09300c5d14e35db0e7476bfb87fac243498273d',
                                                                             'size': 703}},
             'sha256': '3af32ef61579bc441883a63fce6b28bc9975df194d72b399415db67c61d5c25a'},
 'libopusfile': {'members': {'libsdl-org-opusfile-9e5322c/AUTHORS': {'sha256': '7bafd4ec8a8f284cce7a8c9dab476f141ed0de91f38947c145e49929d0922071',
                                                                     'size': 262},
                             'libsdl-org-opusfile-9e5322c/COPYING': {'sha256': '0267ae795ab744c4e0f9c45e249440fdf2e75dac8c804f36066b28649bf74aaf',
                                                                     'size': 1487}},
                 'sha256': 'ed9ea07e9e7e9f9b603e793ffac9551218c978cd5021c7d8f9a3a5dfece9a860'},
 'sdl3-mixer': {'members': {'SDL3_mixer-3.2.4/LICENSE.txt': {'sha256': 'e36997fe492c48e7ed873065cc3a694f38e8189a328d3709099ffdc9a8843f4c',
                                                             'size': 878},
                            'SDL3_mixer-3.2.4/src/dr_libs/dr_mp3.h': {'sha256': 'f0919e1652ffd9469e3440b5c759d26e862a50e6dc93cb4b51cb7e7e90b00f77',
                                                                      'size': 208336},
                            'SDL3_mixer-3.2.4/src/stb_vorbis/stb_vorbis.h': {'sha256': 'e0ddb968df34374d4253fa9d7b4a8bb757cba8e08a91fd0dfc6faa4abedcda2a',
                                                                             'size': 201104}},
                'sha256': '182a07c745375e113dc740d43964ff21b0be29f29f59876c4dbc4db3d32f6901'}}


# Use the host's glibc/loader so its graphics-driver modules can load against
# their matching system libc. Application libraries and plugins are bundled.
HOST_GLIBC = frozenset({"libc.so.6", "libm.so.6", "libpthread.so.0", "libdl.so.2",
                        "librt.so.1", "libresolv.so.2", "libutil.so.1",
                        "ld-linux-x86-64.so.2"})


# This recipe applies only to one immutable producer artifact. The unchanged
# ELF inspector validates the derived bytes; no other search path is normalized.
PULSE_RECIPE = {
    "id": "debian12-libpulse-origin-v1",
    "image": "ghcr.io/atrinik/classic-portable-build@sha256:8f6d345f0e24afad5e53d9e35c37f3e3b4012e5cc2c399334711e55e61a2a338",
    "platform_manifest": "sha256:7b7f36fef585c5424a56edbe2939a25ab1facbb3dd17a2f3a55980866dd85486",
    "provider_path": "/usr/lib/x86_64-linux-gnu/libpulse.so.0.24.2",
    "input_sha256": "38b07a06cefcaabd8db3edaddafea0ab5b5d2ee15e8641929f7d07d45fb66d59",
    "output_sha256": "43da0e3a6816292d4c4c973d56d2753e968a28bb020f8b3035e404044513acf2",
    "size": 338992,
    "offset": 33230,
    "original": "/usr/lib/x86_64-linux-gnu/pulseaudio",
    "replacement": "$ORIGIN",
    "span": 37,
}


def pulse_derivative(data: bytes) -> bytes:
    """Reproduce the fixed in-place dynstr edit without changing any ELF offsets."""
    recipe = PULSE_RECIPE
    if (len(data) != recipe["size"] or
            hashlib.sha256(data).hexdigest() != recipe["input_sha256"]):
        raise export.ExportError("portable-derivative: pinned libpulse input differs")
    original = recipe["original"].encode() + b"\0"
    start, length = recipe["offset"], recipe["span"]
    if len(original) != length or data.count(original) != 1 or data[start:start + length] != original:
        raise export.ExportError("portable-derivative: pinned libpulse string span differs")
    replacement = (recipe["replacement"].encode() + b"\0").ljust(length, b"\0")
    result = data[:start] + replacement + data[start + length:]
    if len(result) != len(data) or hashlib.sha256(result).hexdigest() != recipe["output_sha256"]:
        raise export.ExportError("portable-derivative: libpulse output digest differs")
    return result


def provider_payload(stack: ExitStack, path: Path, descriptor: int, record: dict):
    """Return the original provider or its sole verified private derivative."""
    if str(path) != PULSE_RECIPE["provider_path"]:
        return descriptor, record, None
    if IMAGE != PULSE_RECIPE["image"] or PLATFORM_MANIFEST != PULSE_RECIPE["platform_manifest"]:
        raise export.ExportError("portable-derivative: producer coordinate differs")
    if record["sha256"] != PULSE_RECIPE["input_sha256"] or record["size"] != PULSE_RECIPE["size"]:
        raise export.ExportError("portable-derivative: pinned libpulse record differs")
    before = os.fstat(descriptor)
    data = os.pread(descriptor, record["size"] + 1, 0)
    if export._identity(os.fstat(descriptor)) != export._identity(before):
        raise export.ExportError("portable-derivative: provider changed during read")
    result = pulse_derivative(data)
    output = os.memfd_create("atrinik-libpulse-derivative", os.MFD_CLOEXEC)
    stack.callback(os.close, output)
    pending = memoryview(result)
    while pending:
        written = os.write(output, pending)
        if written <= 0:
            raise export.ExportError("portable-derivative: incomplete private copy")
        pending = pending[written:]
    os.fchmod(output, 0o700 if record["executable"] else 0o600)
    derived = byte_record(output)
    if derived != {**record, "sha256": PULSE_RECIPE["output_sha256"]}:
        raise export.ExportError("portable-derivative: private copy differs")
    return output, derived, dict(PULSE_RECIPE)


def derivative_recipe_script() -> bytes:
    """Ship a standalone reproducible recipe alongside corresponding sources."""
    return ("#!/usr/bin/env python3\n"
            "# Atrinik libpulse export modification, 2026. MIT; see wrapper sources.\n"
            "import hashlib, pathlib, sys\n"
            "recipe = " + repr(PULSE_RECIPE) + "\n"
            "source, destination = map(pathlib.Path, sys.argv[1:])\n"
            "data = source.read_bytes()\n"
            "if len(data) != recipe['size'] or hashlib.sha256(data).hexdigest() != recipe['input_sha256']:\n"
            "    raise SystemExit('pinned libpulse input differs')\n"
            "offset, span = recipe['offset'], recipe['span']\n"
            "replacement = (recipe['replacement'].encode() + bytes([0])).ljust(span, bytes([0]))\n"
            "result = data[:offset] + replacement + data[offset + span:]\n"
            "if len(result) != len(data) or hashlib.sha256(result).hexdigest() != recipe['output_sha256']:\n"
            "    raise SystemExit('derived libpulse output differs')\n"
            "with destination.open('xb') as stream:\n"
            "    stream.write(result)\n").encode()


def byte_record(descriptor: int) -> dict:
    before = os.fstat(descriptor)
    if not stat.S_ISREG(before.st_mode) or not 0 <= before.st_size <= 16 * 1024**3:
        raise export.ExportError("portable-source: bounded regular file required")
    digest = hashlib.sha256()
    offset = 0
    while data := os.pread(descriptor, min(1024 * 1024, before.st_size - offset + 1), offset):
        offset += len(data)
        if offset > before.st_size:
            raise export.ExportError("portable-source: file grew while hashing")
        digest.update(data)
    if offset != before.st_size or export._identity(os.fstat(descriptor)) != export._identity(before):
        raise export.ExportError("portable-source: file changed while hashing")
    return {"sha256": digest.hexdigest(), "size": offset,
            "executable": bool(before.st_mode & 0o111)}


def _read_regular(path: Path, *, limit: int = export.MAX_MANIFEST_BYTES) -> bytes:
    with ExitStack() as stack:
        parent = export._open_root(path.parent)
        stack.callback(os.close, parent)
        descriptor = export._open_beneath(parent, path.name)
        stack.callback(os.close, descriptor)
        before = os.fstat(descriptor)
        if not stat.S_ISREG(before.st_mode) or before.st_size > limit:
            raise export.ExportError("portable-metadata: bounded regular file required")
        data = os.pread(descriptor, limit + 1, 0)
        if len(data) != before.st_size or export._identity(os.fstat(descriptor)) != export._identity(before):
            raise export.ExportError("portable-metadata: changed during read")
        return data


LLVM_ROOT = "/opt/llvm-23.1.3"
PORTABLE_C_FLAGS = "-O2 -march=x86-64 -mtune=generic --no-default-config"
PORTABLE_RELEASE_FLAGS = "-O2 -DNDEBUG"
PORTABLE_LINK_FLAGS = "-L" + LLVM_ROOT + "/lib --rtlib=compiler-rt --unwindlib=libunwind -fuse-ld=lld"


def portable_configure_arguments(source: Path) -> list[str]:
    """Pin CMake's full compiler, optimization and linker contract."""
    settings = {
        "CMAKE_SKIP_RPATH": "ON", "CMAKE_BUILD_TYPE": "Release",
        "CMAKE_TOOLCHAIN_FILE": str(source / "cmake/toolchains/llvm.cmake"),
        "CMAKE_EXPORT_COMPILE_COMMANDS": "ON",
        "CMAKE_C_LINKER_LAUNCHER": "", "CMAKE_CXX_LINKER_LAUNCHER": "",
        "CMAKE_C_COMPILER_LAUNCHER": "", "CMAKE_CXX_COMPILER_LAUNCHER": "",
        "CMAKE_C_COMPILER": LLVM_ROOT + "/bin/clang",
        "CMAKE_CXX_COMPILER": LLVM_ROOT + "/bin/clang++",
        "CMAKE_LINKER": LLVM_ROOT + "/bin/ld.lld",
        "CMAKE_AR": LLVM_ROOT + "/bin/llvm-ar",
        "CMAKE_RANLIB": LLVM_ROOT + "/bin/llvm-ranlib",
        "CMAKE_NM": LLVM_ROOT + "/bin/llvm-nm",
        "CMAKE_STRIP": LLVM_ROOT + "/bin/llvm-strip",
        "CMAKE_OBJCOPY": LLVM_ROOT + "/bin/llvm-objcopy",
        "CMAKE_OBJDUMP": LLVM_ROOT + "/bin/llvm-objdump",
        "CMAKE_READELF": LLVM_ROOT + "/bin/llvm-readelf",
        "CMAKE_C_FLAGS": PORTABLE_C_FLAGS, "CMAKE_CXX_FLAGS": PORTABLE_C_FLAGS,
        "CMAKE_C_FLAGS_RELEASE": PORTABLE_RELEASE_FLAGS,
        "CMAKE_CXX_FLAGS_RELEASE": PORTABLE_RELEASE_FLAGS,
        "CMAKE_EXE_LINKER_FLAGS": PORTABLE_LINK_FLAGS,
        "CMAKE_SHARED_LINKER_FLAGS": PORTABLE_LINK_FLAGS,
        "CMAKE_MODULE_LINKER_FLAGS": PORTABLE_LINK_FLAGS,
        "CMAKE_EXE_LINKER_FLAGS_RELEASE": "",
        "CMAKE_SHARED_LINKER_FLAGS_RELEASE": "",
        "CMAKE_MODULE_LINKER_FLAGS_RELEASE": "",
        "CMAKE_INTERPROCEDURAL_OPTIMIZATION": "OFF",
        "CMAKE_INTERPROCEDURAL_OPTIMIZATION_RELEASE": "OFF",
    }
    return ["-D" + name + "=" + value for name, value in settings.items()]


def _portable_command(words: list[str], cache: dict[str, str], *, link: bool) -> None:
    from .model import WorkspaceError
    compilers = {LLVM_ROOT + "/bin/clang", LLVM_ROOT + "/bin/clang++"}
    if not words:
        raise WorkspaceError("portable compile/link invocation is missing")
    if any(cache.get(name) for name in ("CMAKE_C_COMPILER_LAUNCHER", "CMAKE_CXX_COMPILER_LAUNCHER")):
        raise WorkspaceError("portable compile/link launcher changed")
    if not words or words[0] not in compilers:
        raise WorkspaceError("portable compile/link compiler changed")
    if (any(word.startswith(("-flto", "--target=", "--config", "-config", "--ld-path", "-ld-path", "-B", "-resource-dir", "--resource-dir",
                                "--sysroot", "-isysroot", "--gcc-toolchain", "-gcc-toolchain",
                                "--gcc-install-dir", "--gcc-triple")) for word in words)
            or any(word.startswith("-m") and word not in {"-march=x86-64", "-mtune=generic", "-m64"} for word in words)
            or words.count("--no-default-config") != 1
            or any(word in {"-Xclang", "-target", "-target-cpu", "-target-feature"} for word in words)
            or [word for word in words if re.fullmatch(r"-O(?:[0-9]|fast|s|z|g)", word)] != ["-O2", "-O2"]
            or [word for word in words if word.startswith("-march=")] != ["-march=x86-64"]
            or [word for word in words if word.startswith("-mtune=")] != ["-mtune=generic"]
            or any(word.startswith("@") for word in words)
            or "-DNDEBUG" not in words):
        raise WorkspaceError("portable compile/link optimization or CPU/LTO contract changed")
    if link:
        search_paths = []
        for index, word in enumerate(words):
            if word == "-L":
                search_paths.append(words[index + 1] if index + 1 < len(words) else "")
            elif word.startswith("-L"):
                search_paths.append(word[2:])
            elif (word.startswith("-Wl,") and any(part.startswith(("-L", "--library-path")) for part in word[4:].split(","))) or (word == "-Xlinker" and index + 1 < len(words)
                    and words[index + 1].startswith(("-L", "--library-path"))):
                raise WorkspaceError("portable indirect linker search path changed")
        if not search_paths or search_paths[0] != LLVM_ROOT + "/lib":
            raise WorkspaceError("portable LLVM runtime search path changed")
    for option, expected in (("fuse-ld", "lld"), ("rtlib", "compiler-rt"), ("unwindlib", "libunwind")):
        family = [word for word in words if re.match(r"^-{1,2}" + option + r"(?:=|$)", word)]
        desired = ("-" if option == "fuse-ld" else "--") + option + "=" + expected
        if (link and (not family or any(word != desired for word in family))) or (not link and family):
            raise WorkspaceError("portable compile/link LLD/runtime contract changed")



def _portable_driver(words: list[str], evidence: dict, binary: Path, *, link: bool) -> None:
    """Bind effective driver tools and code generation to the verified cohort."""
    from .model import WorkspaceError
    try:
        output = export._tool_report([words[0], "-###", *words[1:]], cwd=binary, timeout=30)
        invocations = [shlex.split(line) for line in output.splitlines() if line.lstrip().startswith('"')]
        if not invocations or any(not args for args in invocations):
            raise ValueError("missing driver invocations")
        compiler = str(Path(evidence["tools"]["compiler"]["path"]).resolve())
        linker = evidence["tools"]["linker"]["path"]
        runtime = evidence["runtime"]["path"]
        cc1 = [args for args in invocations if len(args) > 1 and args[1] == "-cc1" and args[0] == compiler]
        links = [args for args in invocations if args[0] == linker]
        if (len(cc1) != (0 if link else 1) or len(links) != (1 if link else 0)
                or len(invocations) != 1):
            raise ValueError("external compiler, assembler, linker or extra tool selected")
        if cc1:
            args = cc1[0]
            for option, expected in (("-target-cpu", "x86-64"), ("-tune-cpu", "generic"),
                    ("-triple", evidence["target"]), ("-resource-dir", LLVM_ROOT + "/lib/clang/23")):
                if args.count(option) != 1 or args[args.index(option) + 1] != expected:
                    raise ValueError("effective CPU, target or compiler resource directory changed")
            if ([arg for arg in args if re.fullmatch(r"-O(?:[0-9]|fast|s|z|g)", arg)] != ["-O2"]
                    or "-emit-obj" not in args or any(arg.startswith(("-flto", "-fno-integrated-as")) for arg in args)
                    or "-target-feature" in args
                    or any(arg.startswith(("-load", "-plugin", "-add-plugin", "-fpass-plugin", "-fplugin", "-mllvm")) for arg in args)):
                raise ValueError("effective code generation changed")
        else:
            args = links[0]
            searches = [arg[2:] for arg in args if arg.startswith("-L")]
            if (not searches or searches[0] != LLVM_ROOT + "/lib" or runtime not in args
                    or "-lunwind" not in args or args.count("-m") != 1
                    or args[args.index("-m") + 1] != "elf_x86_64"
                    or any(("libclang_rt.builtins" in arg and arg != runtime)
                           or ("libunwind" in arg and arg != evidence["unwinder"]["path"])
                           or arg in {"-lgcc", "-lgcc_s"} or re.search(r"libgcc[^/]*\.(?:a|so)", arg)
                           or arg.startswith(("--plugin", "-plugin", "--load-pass-plugin", "-load-pass-plugin", "--lto", "-lto")) for arg in args)):
                raise ValueError("effective linker or compiler runtime changed")
    except (OSError, ValueError, IndexError, KeyError, TypeError, export.ExportError, subprocess.SubprocessError) as error:
        raise WorkspaceError("portable effective compiler/linker contract changed") from error

def validate_portable_build(binary: Path, evidence: dict) -> bytes:
    """Check cached settings and generated compile/link commands before export."""
    from .model import WorkspaceError
    raw = _read_regular(binary / "CMakeCache.txt")
    values = {}
    for line in raw.decode("utf-8").splitlines():
        if line.startswith(("#", "//")) or "=" not in line or ":" not in line.split("=", 1)[0]:
            continue
        key, value = line.split("=", 1)
        name = key.split(":", 1)[0]
        if name in values:
            raise WorkspaceError("portable CMake configuration changed: duplicate " + name)
        values[name] = value
    try:
        identity = json.loads(_read_regular(binary / ".atrinik-configure.json"), object_pairs_hook=export._unique_object)
        source = Path(identity["source"]["path"])
        if not source.is_absolute() or values.get("CMAKE_HOME_DIRECTORY") != str(source):
            raise ValueError("source identity changed")
    except (OSError, ValueError, KeyError, TypeError) as error:
        raise WorkspaceError("portable CMake configuration changed: source identity is unavailable or invalid") from error
    for argument in portable_configure_arguments(source):
        name, expected = argument[2:].split("=", 1)
        if values.get(name) != expected:
            raise WorkspaceError("portable CMake configuration changed: " + name)
    try:
        commands = json.loads(_read_regular(binary / "compile_commands.json"), object_pairs_hook=export._unique_object)
        if not isinstance(commands, list) or not commands:
            raise ValueError("missing compile commands")
        for record in commands:
            words = record.get("arguments") or shlex.split(record["command"])
            if not isinstance(words, list) or not all(isinstance(word, str) for word in words):
                raise ValueError("invalid command")
            if record.get("directory") != str(binary):
                raise ValueError("compile working directory changed")
            _portable_command(words, values, link=False)
            _portable_driver(words, evidence, binary, link=False)
        result = subprocess.run(["/usr/bin/ninja", "-C", str(binary), "-t", "commands", "atrinik"],
                                capture_output=True, text=True, timeout=30, check=True,
                                env={"PATH": LLVM_ROOT + "/bin:/usr/bin:/bin", "LC_ALL": "C"})
        if len(result.stdout) > export.MAX_MANIFEST_BYTES:
            raise ValueError("command report too large")
        links = []
        for line in result.stdout.splitlines():
            words = shlex.split(line)
            if ("-o" in words and "-c" not in words and words.index("-o") + 1 < len(words)
                    and (Path(words[words.index("-o") + 1]).name == "atrinik"
                         or ".so" in Path(words[words.index("-o") + 1]).suffixes)):
                command = words[2:] if words[:2] == [":", "&&"] else words
                if "&&" in command:
                    tail = command[command.index("&&") + 1:]
                    if tail != [":"]:
                        raise ValueError("unexpected linker command suffix")
                    command = command[:command.index("&&")]
                links.append(command)
        if not links:
            raise ValueError("missing link commands")
        for command in links:
            _portable_command(command, values, link=True)
            _portable_driver(command, evidence, binary, link=True)
    except (OSError, ValueError, KeyError, TypeError, UnicodeError, subprocess.SubprocessError) as error:
        raise WorkspaceError("portable compile/link evidence is unavailable or invalid") from error
    return raw


def _verify_compiler_bytes(evidence: dict) -> None:
    """Rehash the selected tools and runtimes before using image metadata."""
    compiler = evidence["tools"]["compiler"]
    cxx = {"path": str(Path(compiler["path"]).with_name("clang++")), "sha256": compiler["sha256"]}
    records = [*evidence["tools"].values(), cxx, evidence["runtime"], evidence["unwinder"]]
    for record in records:
        requested = Path(record["path"])
        try:
            resolved = requested.resolve(strict=True)
            with ExitStack() as stack:
                parent = export._open_root(resolved.parent)
                stack.callback(os.close, parent)
                descriptor = export._open_beneath(parent, resolved.name)
                stack.callback(os.close, descriptor)
                if (byte_record(descriptor)["sha256"] != record["sha256"]
                        or requested.resolve(strict=True) != resolved):
                    raise export.ExportError("portable-compiler: selected tool or runtime bytes differ")
        except (OSError, RuntimeError) as error:
            raise export.ExportError("portable-compiler: selected tool or runtime unavailable") from error


def installed_metadata() -> tuple[dict[str, bytes], dict[str, dict]]:
    root = Path("/opt/atrinik-portable")
    if set(METADATA_HASHES) != export.PORTABLE_METADATA_FILES:
        raise export.ExportError(
            "portable-producer: LLVM contract requires a newly verified immutable producer handoff")
    raw = {name: _read_regular(root / name) for name in METADATA_HASHES}
    export.portable_metadata_report(raw, expected_hashes=METADATA_HASHES,
                                   immutable_image=IMAGE, runnable_manifest=PLATFORM_MANIFEST,
                                   consumer_commit=CONSUMER_COMMIT)
    documents = {name: json.loads(data) for name, data in raw.items()}
    _verify_compiler_bytes(documents["compiler-evidence.json"])
    return raw, documents


def _output_parent(path: Path) -> int:
    """Require trusted output ancestry, allowing only root-owned sticky anchors."""
    descriptor = os.open(path.anchor, os.O_RDONLY | os.O_DIRECTORY | os.O_NOFOLLOW)
    try:
        for part in (None, *path.parts[1:]):
            if part is not None:
                child = os.open(part, os.O_RDONLY | os.O_DIRECTORY | os.O_NOFOLLOW, dir_fd=descriptor)
                os.close(descriptor)
                descriptor = child
            info = os.fstat(descriptor)
            if info.st_uid not in (0, os.geteuid()) or (info.st_mode & 0o022 and
                    not (info.st_uid == 0 and info.st_mode & stat.S_ISVTX)):
                raise export.ExportError("portable-output: unsafe ancestor owner or mode")
        return descriptor
    except BaseException:
        os.close(descriptor)
        raise


class Publication:
    """Exclusive private staging with descriptor-relative no-replace publication."""

    def __init__(self, destination: Path, sources: dict[str, str]):
        from .workspace import rename_no_replace_at
        self.rename = rename_no_replace_at
        self.destination = destination
        if not destination.is_absolute() or destination.name in ("", ".", ".."):
            raise export.ExportError("portable-output: absolute new directory required")
        self.parent = _output_parent(destination.parent)
        self.parent_identity = os.fstat(self.parent)
        self.descriptor = -1
        self.name = ".atrinik-export-" + secrets.token_hex(16)
        self.path = destination.parent / self.name
        self.manifest = {"schema_version": 1, "files": {}, "sources": sources,
                         "application_libraries": [], "notices": []}
        try:
            if os.fstat(self.parent).st_uid != os.geteuid() or os.fstat(self.parent).st_mode & 0o022:
                raise export.ExportError("portable-output: parent must be private to the publishing user")
            try:
                os.stat(destination.name, dir_fd=self.parent, follow_symlinks=False)
            except FileNotFoundError:
                pass
            else:
                raise export.ExportError("portable-output: destination already exists")
            os.mkdir(self.name, 0o700, dir_fd=self.parent)
            self.descriptor = os.open(self.name, os.O_RDONLY | os.O_DIRECTORY | os.O_NOFOLLOW,
                                      dir_fd=self.parent)
            self.identity = os.fstat(self.descriptor)
        except BaseException:
            self.close()
            raise

    def close(self) -> None:
        if self.descriptor >= 0:
            os.close(self.descriptor)
            self.descriptor = -1
        if self.parent >= 0:
            os.close(self.parent)
            self.parent = -1

    def __enter__(self):
        return self

    def __exit__(self, *_):
        self.close()

    def _create(self, name: str) -> int:
        parts = export.relative_path(name).split("/")
        parent = os.dup(self.descriptor)
        try:
            for part in parts[:-1]:
                try:
                    os.mkdir(part, 0o755, dir_fd=parent)
                except FileExistsError:
                    pass
                child = os.open(part, os.O_RDONLY | os.O_DIRECTORY | os.O_NOFOLLOW, dir_fd=parent)
                os.close(parent)
                parent = child
            return os.open(parts[-1], os.O_RDWR | os.O_CREAT | os.O_EXCL | os.O_NOFOLLOW,
                           0o600, dir_fd=parent)
        finally:
            os.close(parent)

    def add(self, name: str, descriptor: int, record: dict, *, library=False, notice=False) -> None:
        if name in self.manifest["files"] or name == export.MANIFEST_NAME:
            raise export.ExportError("portable-output: duplicate payload destination")
        output = self._create(name)
        try:
            copied = export.copy_payload(descriptor, output, sha256=record["sha256"],
                                         size=record["size"], executable=record["executable"])
        finally:
            os.close(output)
        self.manifest["files"][name] = copied
        if library:
            self.manifest["application_libraries"].append(name)
        if notice:
            self.manifest["notices"].append(name)

    def add_bytes(self, name: str, data: bytes, *, executable=False, notice=False) -> None:
        # memfd is private, regular, and has no temporary pathname to race.
        descriptor = os.memfd_create("atrinik-export-input", os.MFD_CLOEXEC)
        try:
            pending = memoryview(data)
            while pending:
                written = os.write(descriptor, pending)
                if written <= 0:
                    raise export.ExportError("portable-output: incomplete generated input")
                pending = pending[written:]
            self.add(name, descriptor, {"sha256": hashlib.sha256(data).hexdigest(),
                                       "size": len(data), "executable": executable}, notice=notice)
        finally:
            os.close(descriptor)

    def add_path(self, name: str, source: Path, expected: dict | None = None,
                 *, library=False, notice=False) -> dict:
        parent = export._open_root(source.parent)
        try:
            descriptor = export._open_beneath(parent, source.name)
            try:
                record = byte_record(descriptor)
                if expected is not None and (record["sha256"] != expected["sha256"] or
                                             record["size"] != expected.get("size", record["size"])):
                    raise export.ExportError("portable-source: expected payload mismatch")
                self.add(name, descriptor, record, library=library, notice=notice)
                return record
            finally:
                os.close(descriptor)
        finally:
            os.close(parent)

    def publish(self, revalidate: Callable[[], None]) -> dict:
        data = (json.dumps(self.manifest, sort_keys=True, separators=(",", ":")) + "\n").encode()
        export.load_manifest(data)
        descriptor = self._create(export.MANIFEST_NAME)
        try:
            with os.fdopen(descriptor, "wb", closefd=False) as stream:
                stream.write(data)
                stream.flush()
            os.fsync(descriptor)
        finally:
            os.close(descriptor)
        export.verify_export(self.path)
        revalidate()
        parent = _output_parent(self.destination.parent)
        try:
            a, b = os.fstat(parent), self.parent_identity
            current = os.stat(self.name, dir_fd=parent, follow_symlinks=False)
            if (a.st_dev, a.st_ino) != (b.st_dev, b.st_ino) or (current.st_dev, current.st_ino) != (self.identity.st_dev, self.identity.st_ino):
                raise export.ExportError("portable-output: parent or staging pathname changed")
        finally:
            os.close(parent)
        # Verify again after the source/build guard and sync every directory.
        export.verify_export(self.path)
        def sync(directory: int):
            with os.scandir(directory) as entries:
                for entry in entries:
                    if entry.is_dir(follow_symlinks=False):
                        child = os.open(entry.name, os.O_RDONLY | os.O_DIRECTORY | os.O_NOFOLLOW, dir_fd=directory)
                        try:
                            sync(child)
                        finally:
                            os.close(child)
            os.fsync(directory)
        sync(self.descriptor)
        self.rename(self.parent, self.name, self.parent, self.destination.name)
        os.fsync(self.parent)
        installed = export._open_root(self.destination)
        try:
            current = os.fstat(installed)
            if (current.st_dev, current.st_ino) != (self.identity.st_dev, self.identity.st_ino):
                raise export.ExportError("portable-output: published pathname changed")
        finally:
            os.close(installed)
        return {"schema_version": 1, "path": str(self.destination),
                "manifest_sha256": hashlib.sha256(data).hexdigest(),
                "files": len(self.manifest["files"]), "sources": self.manifest["sources"]}


def symbol_closure(objects: dict[str, dict], *, entrypoint: str = "bin/atrinik") -> dict:
    """Check dynamic symbols in the executable and each declared plugin scope."""
    if entrypoint not in objects:
        raise export.ExportError("portable-symbols: missing entrypoint")
    providers = {}
    for path, facts in objects.items():
        symbols = facts.get("symbols")
        if not isinstance(symbols, dict) or not isinstance(symbols.get("defined"), list):
            raise export.ExportError("portable-symbols: missing dynamic symbol definitions")
        name = facts.get("soname") or Path(path).name
        if name in providers:
            raise export.ExportError("portable-symbols: ambiguous provider")
        providers[name] = path
    def scope(root):
        seen = set()
        pending = [root]
        while pending:
            path = pending.pop()
            if path in seen:
                continue
            seen.add(path)
            for dependency in objects[path].get("needed", []):
                if dependency not in providers:
                    raise export.ExportError("portable-symbols: unresolved dependency " + dependency)
                pending.append(providers[dependency])
        return seen
    initial = scope(entrypoint)
    for name, facts in objects.items():
        # A dlopen object sees the executable's initial global scope and its
        # own dependencies, never arbitrary unrelated optional plugins.
        loaded = initial | scope(name)
        definitions = set().union(*(set(objects[path]["symbols"]["defined"]) for path in loaded))
        missing = set(facts["symbols"]["required"]) - definitions
        if missing:
            raise export.ExportError("portable-symbols: unresolved symbols in " + name + ": " + ", ".join(sorted(missing)[:8]))
    return {"objects": len(objects), "symbol_names_verified": True}


def add_regular_tree(publication: Publication, source: Path, destination: str,
                     *, notice=False, expected: dict[str, dict] | None = None) -> dict:
    descriptor = export._open_root(source)
    try:
        files, identities = export._inventory(descriptor)
        copied = {}
        if expected is not None and set(files) != set(expected):
            raise export.ExportError("portable-source: source proof inventory differs from payload")
        for name in sorted(files):
            payload = export._open_beneath(descriptor, name)
            try:
                record = byte_record(payload)
                if expected is not None and (name not in expected or
                        record["sha256"] != expected[name]["sha256"] or
                        record["size"] != expected[name]["size"] or
                        record["executable"] != expected[name]["executable"]):
                    raise export.ExportError("portable-source: source proof disagrees with payload")
                publication.add(destination + "/" + name, payload, record, notice=notice)
                copied[name] = record
            finally:
                os.close(payload)
        if export._inventory(descriptor) != (files, identities):
            raise export.ExportError("portable-source: tree changed during copy")
        return copied
    finally:
        os.close(descriptor)


def portable_executable(descriptor: int) -> dict:
    facts = export.inspect_elf(descriptor)
    if (facts["type"] not in {"EXEC", "DYN"} or facts["soname"] is not None or
            facts["interpreter"] != "/lib64/ld-linux-x86-64.so.2" or
            not byte_record(descriptor)["executable"]):
        raise export.ExportError("portable-runtime: executable requires the supported host loader and executable mode")
    return facts


def add_runtime(publication: Publication, documents: dict, binary: Path) -> dict:
    """Hash each installed provider against authenticated producer ABI records."""
    rows = documents["runtime-abi.json"]["objects"]
    inspected = {}
    copied = {}
    by_path = {}
    by_soname = {}
    derivatives = []
    with ExitStack() as stack:
        for row in rows:
            path = Path(row["path"])
            parent = export._open_root(path.parent)
            stack.callback(os.close, parent)
            descriptor = export._open_beneath(parent, path.name)
            stack.callback(os.close, descriptor)
            record = byte_record(descriptor)
            if record["sha256"] != row["sha256"]:
                raise export.ExportError("portable-runtime: provider hash mismatch")
            descriptor, record, derivative = provider_payload(stack, path, descriptor, record)
            try:
                facts = export.inspect_elf(descriptor)
            except export.ExportError as error:
                raise export.ExportError("portable-runtime: provider " + str(path) + ": " + str(error)) from error
            if set(facts["needed"]) != set(row["needed"]):
                raise export.ExportError("portable-runtime: dependency metadata differs from ELF")
            basename = facts["soname"] or path.name
            if basename in by_path:
                raise export.ExportError("portable-runtime: ambiguous provider SONAME")
            by_path[basename] = row["path"]
            target = "lib/ossl-modules/legacy.so" if path.name == "legacy.so" else "lib/" + basename
            inspected[target] = facts
            by_soname[basename] = facts
            if basename not in HOST_GLIBC:
                publication.add(target, descriptor, record, library=True)
                copied[row["path"]] = target
                if derivative is not None:
                    derivatives.append({**derivative, "exported_path": target})
        source = export._open_root(binary.parent)
        stack.callback(os.close, source)
        client = export._open_beneath(source, binary.name)
        stack.callback(os.close, client)
        inspected["bin/atrinik"] = portable_executable(client)
        publication.add("bin/atrinik", client, byte_record(client))
        # Check all version providers against their actual hashed ELF bytes,
        # including the glibc baseline. Runtime still uses the host glibc.
        dependency = export.elf_dependency_report(inspected, entrypoint="bin/atrinik",
                                                   host_libraries=frozenset())
        symbols = symbol_closure(inspected)
        excluded = {(row["object"], row["feature"], tuple(row["soname"]))
                    for row in documents["contract.json"]["runtime"]["unsupported_dlopen_features"]}
        dynamic = []
        for row in rows:
            for feature in row["dlopen"]:
                if (row["path"], feature["feature"], tuple(feature["soname"])) in excluded:
                    continue
                candidates = row["dlopen_providers"][feature["feature"]]
                for soname in feature["soname"]:
                    provider = by_path.get(soname)
                    if provider is None or provider not in candidates or provider not in copied:
                        raise export.ExportError("portable-runtime: unresolved dynamic SONAME " + soname)
                    dynamic.append({"object": row["path"], "soname": soname, "provider": copied[provider]})
        for row in rows:
            for symbol, requirement in row["required_providers"].items():
                provider = by_soname[requirement["provider"]]
                if symbol not in provider["symbols"]["defined"]:
                    raise export.ExportError("portable-runtime: required symbol absent from its version provider")
    return {"producer": IMAGE, "platform_manifest": PLATFORM_MANIFEST,
            "source_commit": CONSUMER_COMMIT, "runtime_payload_verified": True,
            "dynamic_sonames": dynamic, "symbols": symbols, "derivatives": derivatives,
            "host_glibc": sorted(HOST_GLIBC), "minimum_host_glibc": "2.36",
            "baseline_dependencies": dependency["dependencies"],
            "host_graphics_drivers_bundled": False,
            "hardware_gameplay_verified": False, "audible_playback_verified": False}


def inventory_digest(records: dict) -> str:
    return hashlib.sha256(json.dumps(records, sort_keys=True, separators=(",", ":")).encode()).hexdigest()


def verified_tree_inventory(source: Path, expected_digest: str) -> dict:
    descriptor = export._open_root(source)
    try:
        files, identities = export._inventory(descriptor)
        records = {}
        for name in sorted(files):
            payload = export._open_beneath(descriptor, name)
            try:
                records[name] = byte_record(payload)
            finally:
                os.close(payload)
        if export._inventory(descriptor) != (files, identities):
            raise export.ExportError("portable-legal: source inventory changed")
        if inventory_digest(records) != expected_digest:
            raise export.ExportError("portable-legal: complete producer/source/notice inventory mismatch")
        return records
    finally:
        os.close(descriptor)


def verified_debian_notices(documents: dict) -> dict:
    source_packages = documents["runtime-sources.json"]["source_packages"]
    records = {}
    for row in documents["debian-sources.json"]:
        if row["source"] not in source_packages:
            continue
        requested = Path(row["notice_directory"]) / "copyright"
        resolved = requested.resolve(strict=True)
        if not resolved.is_relative_to("/usr/share/doc"):
            raise export.ExportError("portable-legal: Debian notice escapes its boundary")
        parent = export._open_root(resolved.parent)
        try:
            descriptor = export._open_beneath(parent, resolved.name)
            try:
                records[str(requested)] = {**byte_record(descriptor), "resolved_path": str(resolved)}
            finally:
                os.close(descriptor)
        finally:
            os.close(parent)
    if inventory_digest(records) != DEBIAN_NOTICES_SHA256:
        raise export.ExportError("portable-legal: complete Debian copyright inventory mismatch")
    return records


def add_audio_notices(publication: Publication, root: Path) -> list[dict]:
    """Materialize pinned archive members without extracting archive paths."""
    result = []
    for component, source in AUDIO_NOTICES.items():
        path = root / "sources" / (source["sha256"] + ".tar.gz")
        raw = _read_regular(path, limit=64 * 1024 * 1024)
        if hashlib.sha256(raw).hexdigest() != source["sha256"]:
            raise export.ExportError("portable-legal: audio source archive differs")
        with tarfile.open(fileobj=io.BytesIO(raw), mode="r:gz") as archive:
            members = {}
            for member in archive:
                if member.name in source["members"]:
                    if member.name in members:
                        raise export.ExportError("portable-legal: duplicate audio notice member")
                    members[member.name] = member
            for name, expected in source["members"].items():
                member = members.get(name)
                if member is None or not member.isfile() or member.size != expected["size"]:
                    raise export.ExportError("portable-legal: audio notice member differs")
                stream = archive.extractfile(member)
                if stream is None:
                    raise export.ExportError("portable-legal: audio notice member unreadable")
                with stream:
                    data = stream.read(expected["size"] + 1)
                if len(data) != expected["size"] or hashlib.sha256(data).hexdigest() != expected["sha256"]:
                    raise export.ExportError("portable-legal: audio notice bytes differ")
                target = "licenses/audio/" + component + "/" + name.split("/", 1)[1]
                publication.add_bytes(target, data, notice=True)
                result.append({"archive_sha256": source["sha256"], "member": name,
                               "path": target, **expected})
    return result


def add_corresponding_sources(publication: Publication, documents: dict) -> dict:
    root = Path("/opt/atrinik-portable")
    producer_files = verified_tree_inventory(root, PRODUCER_FILES_SHA256)
    notices = verified_debian_notices(documents)
    archives = documents["runtime-sources.json"]["archives"]
    for name, digest in archives.items():
        publication.add_path("sources/debian/" + name, root / "sources/debian" / name,
                             {"sha256": digest})
    contract = documents["contract.json"]
    for source in contract["sources"]:
        filename = source["url"].rsplit("/", 1)[-1]
        publication.add_path("sources/upstream/" + filename, root / "sources" / (source["sha256"] + ".tar.gz"),
                             {"sha256": source["sha256"]})
    # Retain the complete producer recipes, licenses, source inputs and audio
    # toolchain archives as installed, with exact output inventory hashes.
    remaining = add_regular_tree(publication, root, "sources/portable-producer", expected=producer_files)
    if not any(name.startswith("notices/") for name in remaining):
        raise export.ExportError("portable-legal: producer notices missing")
    for name in remaining:
        if name.startswith("notices/"):
            publication.manifest["notices"].append("sources/portable-producer/" + name)
    source_packages = documents["runtime-sources.json"]["source_packages"]
    for row in documents["debian-sources.json"]:
        if row["source"] not in source_packages:
            continue
        requested = str(Path(row["notice_directory"]) / "copyright")
        record = notices[requested]
        publication.add_path("licenses/debian/" + row["package"].replace(":", "_") + "/copyright",
                             Path(record["resolved_path"]), record, notice=True)
    common_records = {}
    for name in COMMON_LICENSE_NAMES:
        path = Path("/usr/share/common-licenses") / name
        parent = export._open_root(path.parent)
        try:
            descriptor = export._open_beneath(parent, name)
            try:
                common_records[name] = byte_record(descriptor)
                publication.add("licenses/common/" + name, descriptor, common_records[name], notice=True)
            finally:
                os.close(descriptor)
        finally:
            os.close(parent)
    if inventory_digest(common_records) != COMMON_LICENSES_SHA256:
        raise export.ExportError("portable-legal: complete common-license inventory mismatch")
    audio_notices = add_audio_notices(publication, root)
    return {"audio_notices": audio_notices, "debian_source_packages": len(source_packages), "debian_archives": len(archives),
            "producer_files": len(remaining), "producer_inventory_sha256": PRODUCER_FILES_SHA256,
            "debian_notices_sha256": DEBIAN_NOTICES_SHA256,
            "common_licenses_sha256": COMMON_LICENSES_SHA256,
            "sources_and_notices_materialized": True}


def export_client(workspace, profile_name: str, destination: Path) -> dict:
    """Build and publish while the existing profile/source/build guards are held."""
    from .model import WorkspaceError
    from .sound import RELEASED_MODE, validate_release_coordinates, verify_release_tree
    from .workspace import CLASSIC_CLIENT_RUNTIME_SOURCE_EXCLUSIONS
    raw, documents = installed_metadata()
    workspace.paths.ensure()
    profile = workspace._load_profile(profile_name, require_file=False)
    if profile["stack"] != "classic" or profile["sound_mode"] != RELEASED_MODE:
        raise WorkspaceError("portable export requires a Classic profile with verified released sound")
    sound_coordinates = validate_release_coordinates(profile["sound_release"])
    with workspace._resolved_profile_operation(
            profile_name, {"client"}, "portable client export",
            materialize_clean_primaries=True) as snapshot:
        selected = snapshot.paths()
        states = snapshot.checkout_states()
        if any(state["dirty"] for state in states.values()):
            raise WorkspaceError("portable export requires clean committed source inputs")
        if states["classic"]["head"] != CONSUMER_COMMIT:
            raise WorkspaceError("portable producer requires Classic " + CONSUMER_COMMIT)
        if sound_coordinates["source_commit"] != states["sound"]["head"]:
            raise WorkspaceError("portable sound release must match the selected sound source commit")
        proofs = {}
        generations = {}
        def prove_sources():
            current = {}
            for role, source in selected.items():
                record = workspace._source_generation_record(source)
                if record is None:
                    raise WorkspaceError("portable export requires immutable primary source generations")
                current[role] = workspace._validate_source_generation_git_closure(
                    Path(states[record["checkout"]]["path"]), source.parent,
                    record["source_tree"], record["tree"], record["source_includes"])
                if role in generations and record != generations[role]:
                    raise WorkspaceError("portable source generation changed before publication")
                generations[role] = record
            return current
        proofs = prove_sources()
        targets = workspace._expand_build_target("client", profile_name)
        build_root = workspace._build_resolved("client", profile_name, False,
                    targets, selected, build_services={"client"},
                    generate_region_maps=False, portable=True)
        with workspace._profile_build_lock(build_root, profile_name):
            # A concurrent rebuild between build return and lock acquisition must
            # not silently replace the pinned portable configure contract.
            binary_directory = workspace._classic_binary_directory(build_root, "client")
            cache = validate_portable_build(binary_directory, documents["compiler-evidence.json"])
            sound_root, sound_record = workspace._prepare_sound(build_root, selected, profile_name)
            if sound_record["source_tree"] != generations["sound"]["source_tree"]:
                raise WorkspaceError("portable sound release must match the selected sound source tree")
            sources = {"atrinik/" + name + "@main": state["head"] for name, state in states.items()}
            sources["atrinik/devcontainer@main"] = PRODUCER_COMMIT
            with Publication(destination, sources) as publication:
                runtime = add_runtime(publication, documents, binary_directory / "atrinik")
                legal = add_corresponding_sources(publication, documents)
                if runtime["derivatives"]:
                    publication.add_bytes("sources/export-recipes/libpulse-origin.py", derivative_recipe_script())
                    publication.add_bytes("licenses/libpulse-export-modification.txt", (
                        "Atrinik modified lib/libpulse.so.0 on 2026-09-14.\n"
                        "The pinned Debian library's absolute DT_RUNPATH is replaced with $ORIGIN\n"
                        "and NUL padding; all ELF sizes, offsets and other bytes are preserved.\n"
                        "The producer library is unchanged. Both hashes and the exact recipe are\n"
                        "in portable-evidence.json; reproduce with sources/export-recipes/libpulse-origin.py.\n"
                        "PulseAudio corresponding sources, Debian copyright and LGPL notices are\n"
                        "included in sources and licenses/debian. The shared library remains replaceable.\n"
                    ).encode(), notice=True)
                for role, source in selected.items():
                    proof = proofs[role]
                    parent = export._open_root(source.parent)
                    try:
                        for name, record in sorted(proof.items()):
                            descriptor = export._open_beneath(parent, name)
                            try:
                                actual = byte_record(descriptor)
                                if actual["sha256"] != record["sha256"] or actual["size"] != record["size"]:
                                    raise export.ExportError("portable-source: source proof changed")
                                publication.add("sources/" + role + "/" + name, descriptor, actual,
                                                notice=Path(name).name.upper().startswith(("LICENSE", "COPYING", "NOTICE", "ATTRIBUTION", "PROVENANCE")))
                                if name.startswith("source/") and role == "client":
                                    relative = name[len("source/"):]
                                    if role == "client" and relative.split("/", 1)[0] in CLASSIC_CLIENT_RUNTIME_SOURCE_EXCLUSIONS:
                                        continue
                                    destination_name = "share/games/atrinik/" + ("sound/" if role == "sound" else "") + relative
                                    publication.add(destination_name, descriptor, actual)
                            finally:
                                os.close(descriptor)
                    finally:
                        os.close(parent)
                sound_files = add_regular_tree(publication, sound_root, "share/games/atrinik/sound")
                for name in sound_files:
                    if Path(name).name.upper().startswith(("LICENSE", "COPYING", "NOTICE", "ATTRIBUTION")):
                        publication.manifest["notices"].append("share/games/atrinik/sound/" + name)
                # OpenSSL's optional legacy provider must resolve inside the
                # relocated application, independently of the producer prefix.
                launcher = export.launcher_script().replace(
                    b"export ATRINIK_CONFIG_DIR LD_LIBRARY_PATH",
                    b"OPENSSL_MODULES=$root/lib/ossl-modules\nexport ATRINIK_CONFIG_DIR LD_LIBRARY_PATH OPENSSL_MODULES")
                publication.add_bytes("atrinik", launcher, executable=True)
                evidence = {"schema_version": 1, "runtime": runtime, "legal": legal,
                            "metadata_sha256": METADATA_HASHES, "sources": sources,
                            "source_proofs": proofs, "profile": profile_name,
                            "sound": {key: value for key, value in sound_record.items() if key != "root"}}
                publication.add_bytes("portable-evidence.json", (json.dumps(evidence, sort_keys=True) + "\n").encode())
                publication.add_bytes("README.txt", (
                    "Atrinik Classic portable client\nRun ./atrinik on x86-64 Linux with glibc 2.36 or newer.\n"
                    "Use the host X11/XWayland session, Vulkan graphics driver and PulseAudio server.\n"
                    "Application libraries are in lib; host graphics drivers and glibc remain external.\n"
                    "Mutable configuration is outside this directory; set ATRINIK_CONFIG_DIR explicitly.\n"
                    "Corresponding sources and build recipes are in sources; notices are in licenses and sources.\n"
                    "Shared LGPL libraries remain replaceable. Hardware gameplay and audible playback\n"
                    "require separate qualification; byte/decode checks do not establish those results.\n"
                ).encode(), notice=True)
                def revalidate():
                    if verify_release_tree(sound_root, sound_coordinates) != sound_record:
                        raise WorkspaceError("portable released sound changed before publication")
                    if prove_sources() != proofs:
                        raise WorkspaceError("portable source closure changed before publication")
                    fresh, _documents = installed_metadata()
                    if fresh != raw:
                        raise WorkspaceError("portable producer metadata changed before publication")
                    if _read_regular(binary_directory / "CMakeCache.txt") != cache:
                        raise WorkspaceError("portable build configuration changed before publication")
                    validate_portable_build(binary_directory, documents["compiler-evidence.json"])
                result = publication.publish(revalidate)
                return {**result, "image": IMAGE, "platform_manifest": PLATFORM_MANIFEST,
                        "runtime": runtime, "legal": legal}
