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
import tempfile
from typing import Callable

from . import linux_export as export
from . import linux_portable_shell as command_shell

IMAGE = "ghcr.io/atrinik/classic-portable-build@sha256:74761e930f5d57face2d81efb0dc50dd4ce32ce1204810e56580bf68a5972ebc"
PLATFORM_MANIFEST = "sha256:5e30957e54277bdaadeeffa42246258dc9df2086f3fb41f2125d041735ac72aa"
PRODUCER_COMMIT = "eb1f6af78216adb4375826cdb70ef78a61ab403a"
CONSUMER_COMMIT = "bd5317a03cdc82f029105c5dc26428b09fcd54b3"
METADATA_HASHES = {
    "compiler-evidence.json": "e0b3642e19203674e89be910e81ea25da78d812c13c8d75b9189c57928dabd88",
    "contract.json": "7434e16002064ceec2140f9f2e2d86490f742bd385c33985f62ffcb92834d7cf",
    "debian-sources.json": "0882c128d989bf4b8491d7ed666b73f9d572a4da201caa155ee711cff5c9bf13",
    "installed.json": "ae80e4fabe38ea5395c7be4597121cf5e306206a93396a4b40667865ecaf05c5",
    "runtime-abi.json": "19ac09c2323114b6ecb80015894954a5a891c2335d6f4f405252cd66515d682f",
    "runtime-sources.json": "01ff08df86fbb1b58e0c147c122c6c23b1008640f987eaf0785d933b5c9a8963",
    "shader-generation.json": "d0196dcdfd2cdf071b9cbd8e38f0e2ee655b07013db86d4975ff3e6e2132eda5",
}
# Complete installed maps accepted through the released SDK collector and
# trusted hosted filesystem equality. Canonical JSON uses sorted keys and compact
# separators; source records contain relative path -> sha256/size/executable. This pins omitted
# archives/recipes/notices as well as bytes, without a caller-supplied override.
PRODUCER_FILES_SHA256 = '9b5021d22ff56aab80bcb7d845885bcc2c120104930d75bab2e733d1e993c078'
DEBIAN_NOTICES_SHA256 = 'b28d492f795de20e334c7fe08931d7c024374b9f89a07111b6c7fdfc63cd131e'
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
    "image": "ghcr.io/atrinik/classic-portable-build@sha256:74761e930f5d57face2d81efb0dc50dd4ce32ce1204810e56580bf68a5972ebc",
    "platform_manifest": "sha256:5e30957e54277bdaadeeffa42246258dc9df2086f3fb41f2125d041735ac72aa",
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
PORTABLE_PATH = LLVM_ROOT + "/bin:/usr/bin:/bin"
PORTABLE_GENERATOR_PYTHON = Path("/usr/bin/python3")
PORTABLE_LINK_FLAGS = "-L" + LLVM_ROOT + "/lib --rtlib=compiler-rt --unwindlib=libunwind -fuse-ld=lld"


def portable_configure_arguments(source: Path) -> list[str]:
    """Pin CMake's full compiler, optimization and linker contract."""
    settings = {
        "PKG_CONFIG_EXECUTABLE": "/usr/bin/pkgconf",
        "BUILD_TESTING": "OFF",
        "ATRINIK_GPU_SHADER_DIRECTORY": "/opt/atrinik-portable/shaders",
        "CMAKE_SKIP_RPATH": "ON", "CMAKE_BUILD_TYPE": "Release",
        "CMAKE_FIND_USE_PACKAGE_REGISTRY": "FALSE",
        "CMAKE_FIND_USE_SYSTEM_PACKAGE_REGISTRY": "FALSE",
        "CMAKE_MAKE_PROGRAM": "/usr/bin/ninja",
        "CMAKE_TOOLCHAIN_FILE": str(source.parent / "cmake/toolchains/llvm.cmake"),
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


def _verify_command_shell_cohort() -> None:
    try:
        command_shell.verify_cohort(image=IMAGE, platform_manifest=PLATFORM_MANIFEST,
                                    producer_commit=PRODUCER_COMMIT, consumer_commit=CONSUMER_COMMIT)
    except command_shell.LinuxPortableShellError as error:
        raise export.ExportError(str(error)) from error


def _capture_command_shell() -> dict:
    _verify_command_shell_cohort()
    try:
        return command_shell.capture_identity()
    except command_shell.LinuxPortableShellError as error:
        raise export.ExportError(str(error)) from error


def _verify_command_shell(receipt: dict) -> None:
    _verify_command_shell_cohort()
    try:
        command_shell.verify_identity(receipt)
    except command_shell.LinuxPortableShellError as error:
        raise export.ExportError(str(error)) from error


class PortableBuildContext:
    """Same-operation source authority and configure/build observations kept in memory.

    Only the leased export caller supplies selected generations and their verified
    inventories. Disk cache and source-view metadata never create this authority.
    """

    def __init__(self, selected: dict[str, Path], proofs: dict, generations: dict, evidence: dict,
                 *, build_tools: dict):
        import copy
        self.selected = {role: Path(path) for role, path in selected.items()}
        self.proofs = copy.deepcopy(proofs)
        self.generations = copy.deepcopy(generations)
        self.evidence = copy.deepcopy(evidence)
        self.producer_build_tools = copy.deepcopy(export.portable_build_tools(build_tools))
        self.build_tools = None
        self.binary = None
        self.source = None
        self.views = {}
        self.authored = {}
        self.authored_roots = {}
        self.authored_directories = set()
        self.authored_inputs = set()
        self.authored_observation = None
        self.generated_directories = set()
        self.generated = {}
        self.expected_shader = None
        self.baseline = None
        self.output = None
        self.target_outputs = None
        self.host_tools = None

    def validate_selected(self, selected: dict[str, Path]) -> None:
        if self.baseline is not None or {role: Path(path) for role, path in selected.items()} != self.selected:
            raise ValueError("portable selected-source context differs from caller snapshot")
        for role in ("client", "protocol", "libatrinik"):
            if role not in self.selected or role not in self.proofs or role not in self.generations:
                raise ValueError("portable source proof context is incomplete")

    def validate_source_binding(self, binary: Path, source: Path) -> None:
        if (not self.views or self.baseline is not None or source != self.source
                or binary != self.build_root / "build/client"):
            raise ValueError("portable configure coordinates differ from caller context")
        self._verify_authored()
        self.verify_host_tools()

    @staticmethod
    def _record(path: Path) -> dict:
        resolved = path.resolve(strict=True)
        parent = export._open_root(resolved.parent)
        try:
            descriptor = export._open_beneath(parent, resolved.name)
            try:
                record = byte_record(descriptor)
                info = os.fstat(descriptor)
                record["identity"] = export._identity(info)
            finally:
                os.close(descriptor)
        finally:
            os.close(parent)
        if path.resolve(strict=True) != resolved:
            raise ValueError("compiler input path changed")
        return record

    @classmethod
    def _capture_generator_python(cls) -> dict:
        """Observe the fixed host tool; system libraries remain host-tool trust."""
        path = PORTABLE_GENERATOR_PYTHON
        resolved = path.resolve(strict=True)
        record = cls._record(path)
        if not record["executable"]:
            raise ValueError("portable shader Python is not executable")
        probe = "import json, sys; print(json.dumps({'implementation': sys.implementation.name, 'version': list(sys.version_info[:3]), 'executable': sys.executable, 'prefix': sys.base_prefix}))"
        try:
            result = subprocess.run([str(path), "-I", "-c", probe], check=True, capture_output=True,
                                    text=True, timeout=30, stdin=subprocess.DEVNULL,
                                    env={"PATH": PORTABLE_PATH, "LC_ALL": "C", "PYTHONNOUSERSITE": "1"})
            version = json.loads(result.stdout)
            numbers = version["version"]
            if (version["implementation"] != "cpython" or version["executable"] != str(path)
                    or not isinstance(numbers, list) or len(numbers) != 3
                    or not all(type(item) is int for item in numbers) or numbers[0] != 3 or numbers[:2] < [3, 11]):
                raise ValueError("unsupported portable shader Python")
        except (OSError, subprocess.SubprocessError, KeyError, TypeError, ValueError) as error:
            raise ValueError("portable shader Python identity is unavailable or invalid") from error
        if path.resolve(strict=True) != resolved or cls._record(path) != record:
            raise ValueError("portable shader Python changed during inspection")
        return {"path": str(path), "resolved_path": str(resolved), **record, "version": version}

    def verify_host_tools(self) -> None:
        # The caller authenticated this complete cohort before creating the context.
        # Never refresh its authority from mutable on-disk metadata during a build.
        _verify_compiler_bytes(self.evidence)
        self.verify_build_tools()
        if self.host_tools is None or set(self.host_tools) != {"shader_python", "command_shell"}:
            raise ValueError("portable host-tool receipt is missing")
        _verify_command_shell(self.host_tools["command_shell"])
        record = self.host_tools["shader_python"]
        path = PORTABLE_GENERATOR_PYTHON
        if (record["path"] != str(path) or record["resolved_path"] != str(path.resolve(strict=True))
                or self._record(path) != {key: value for key, value in record.items()
                                          if key not in {"path", "resolved_path", "version"}}):
            raise ValueError("portable shader Python executable changed")

    @staticmethod
    def _build_tool_path_protected(info: os.stat_result) -> bool:
        return info.st_uid == 0 and not info.st_mode & (stat.S_IWGRP | stat.S_IWOTH)

    @classmethod
    def _build_tool_record(cls, expected: dict) -> dict:
        """Admit executable bytes before any tool probe; OS dependencies stay trusted."""
        path = Path(expected["path"])
        # Debian's fixed CMake/Ninja/pkgconf paths are regular files. Following a link,
        # even into a protected target, could admit a writable execution chain.
        if not path.is_absolute() or str(path) != expected["resolved_path"]:
            raise ValueError("portable producer build tool resolution changed")
        parent = os.open(path.anchor, os.O_RDONLY | os.O_DIRECTORY | os.O_NOFOLLOW)
        try:
            for part in path.parts[1:-1]:
                if not cls._build_tool_path_protected(os.fstat(parent)):
                    raise ValueError("portable producer build tool ancestor is not root-owned and protected")
                child = os.open(part, os.O_RDONLY | os.O_DIRECTORY | os.O_NOFOLLOW, dir_fd=parent)
                os.close(parent)
                parent = child
            if not cls._build_tool_path_protected(os.fstat(parent)):
                raise ValueError("portable producer build tool ancestor is not root-owned and protected")
            descriptor = export._open_beneath(parent, path.name)
            try:
                info = os.fstat(descriptor)
                if (not stat.S_ISREG(info.st_mode) or not cls._build_tool_path_protected(info)
                        or info.st_mode & (stat.S_ISUID | stat.S_ISGID)):
                    raise ValueError("portable producer build tool is not a protected regular executable")
                if info.st_size != expected["size"] or bool(info.st_mode & 0o111) != expected["executable"]:
                    raise ValueError("portable producer build tool bytes differ")
                record = byte_record(descriptor)
                record["identity"] = export._identity(info)
                if export._identity(os.fstat(descriptor)) != record["identity"]:
                    raise ValueError("portable producer build tool changed during inspection")
            finally:
                os.close(descriptor)
        except OSError as error:
            raise ValueError("portable producer build tool path is unavailable or unsafe") from error
        finally:
            os.close(parent)
        if (path.resolve(strict=True) != path or export._identity(path.stat(follow_symlinks=False)) != record["identity"]
                or any(record[key] != expected[key] for key in ("sha256", "size", "executable"))):
            raise ValueError("portable producer build tool bytes differ")
        return record

    def _capture_build_tools(self) -> dict:
        # Authenticate every tool before running any --version command.
        records = {name: self._build_tool_record(expected)
                   for name, expected in self.producer_build_tools.items()}
        for name, expected in self.producer_build_tools.items():
            for tool, trusted in self.producer_build_tools.items():
                if self._build_tool_record(trusted) != records[tool]:
                    raise ValueError("portable producer build tool changed before version inspection")
            version = export._tool_report([expected["path"], "--version"])
            if version != expected["version"]:
                raise ValueError("portable producer build tool version differs")
            for tool, trusted in self.producer_build_tools.items():
                if self._build_tool_record(trusted) != records[tool]:
                    raise ValueError("portable producer build tool changed during version inspection")
        return {name: {**expected, **records[name]}
                for name, expected in self.producer_build_tools.items()}

    def verify_build_tools(self) -> None:
        if self.build_tools is None or set(self.build_tools) != set(self.producer_build_tools):
            raise ValueError("portable producer build tool receipt is missing")
        for name, expected in self.producer_build_tools.items():
            current = {**expected, **self._build_tool_record(expected)}
            if current != self.build_tools[name]:
                raise ValueError("portable producer build tool executable changed")

    def bind_source_views(self, build_root: Path, views: dict[str, Path]) -> None:
        if self.views or set(views) != {"client", "protocol", "libatrinik"}:
            raise ValueError("portable source view binding is missing or repeated")
        self.build_root = Path(build_root)
        if not self.build_root.is_absolute() or self.build_root.resolve() != self.build_root:
            raise ValueError("portable build namespace is not canonical")
        for role, requested in views.items():
            view = Path(requested)
            generation = self.generations[role]
            namespace = role if generation["source_includes"] else ""
            allowed = ({self.build_root / "sources/client", self.build_root / "sources/client-layout/client"}
                       if role == "client" else
                       {self.build_root / "sources" / namespace / ("input" if namespace else role)})
            if view not in allowed or view.resolve() != view:
                raise ValueError("portable source view is outside the caller's build namespace")
            self.views[role] = view
            for name, expected in self.proofs[role].items():
                relative = export.relative_path(name)
                target = view / relative[7:] if relative.startswith("source/") else view.parent / relative
                origin = self.selected[role].parent / relative
                root = view if relative.startswith("source/") else view.parent / relative.split("/", 1)[0]
                self.authored_roots.setdefault(root, set()).add(target)
                if target in self.authored and self.authored[target][1] != expected:
                    raise ValueError("ambiguous source proof mapping")
                self.authored[target] = (origin, expected)
        self.source = self.views["client"]
        self._verify_authored()
        if self.source.parent / "cmake/toolchains/llvm.cmake" not in self.authored:
            raise ValueError("portable toolchain is outside the caller's supplemental source proof")
        shell = _capture_command_shell()
        _verify_compiler_bytes(self.evidence)
        self.build_tools = self._capture_build_tools()
        _verify_compiler_bytes(self.evidence)
        _verify_command_shell(shell)
        self.host_tools = {"command_shell": shell, "shader_python": self._capture_generator_python()}
        self.verify_host_tools()

    def _verify_authored(self) -> None:
        reserved = {".atrinik-workspace-managed.json", ".atrinik-source-view.json",
                    ".atrinik-source-includes.json"}
        observed = set()
        for root in self.authored_roots:
            if root.is_file():
                observed.add(root)
                continue
            pending = [root]
            while pending:
                directory = pending.pop()
                for entry in directory.iterdir():
                    if entry.name in reserved or (directory == self.views["client"] and entry.name in {".git", "build", "sound"}):
                        continue
                    if entry.is_dir():
                        if not any(path.is_relative_to(entry) for path in self.authored):
                            raise ValueError("unrecorded authored source directory")
                        pending.append(entry)
                    else:
                        observed.add(entry)
                        if len(observed) > export.MAX_FILES:
                            raise ValueError("source inventory exceeds bound")
        if observed != set(self.authored):
            raise ValueError("authored source view inventory changed")
        directories = set()
        inputs = set()
        records = {}
        for requested, (origin, expected) in self.authored.items():
            resolved = requested.resolve(strict=True)
            if resolved not in {requested, origin.resolve(strict=True)}:
                raise ValueError("authored source alias escapes its proven origin")
            record = self._record(requested)
            if any(record[key] != expected[key] for key in ("sha256", "size")):
                raise ValueError("authored source view bytes differ from generation proof")
            records[requested] = record
            inputs.update((requested, resolved))
            parent = requested.parent
            while any(parent == root or parent.is_relative_to(root) for root in self.authored_roots if root.is_dir()):
                directories.update((parent, parent.resolve()))
                parent = parent.parent
        if self.authored_observation is not None and records != self.authored_observation:
            raise ValueError("authored source file identity changed")
        self.authored_observation = records
        self.authored_directories = directories
        self.authored_inputs = inputs

    def _ninja(self, binary: Path) -> str:
        self.verify_host_tools()
        try:
            result = subprocess.run([self.build_tools["ninja"]["path"], "-C", str(binary), "-t", "commands", "atrinik"],
                                    capture_output=True, text=True, timeout=30, check=True,
                                    stdin=subprocess.DEVNULL, env={"PATH": PORTABLE_PATH, "LC_ALL": "C"})
        except (OSError, subprocess.SubprocessError) as error:
            raise ValueError("portable Ninja report is unavailable") from error
        self.verify_host_tools()
        if len(result.stdout) > export.MAX_MANIFEST_BYTES:
            raise ValueError("Ninja report exceeds bound")
        return result.stdout

    def _generated_inventory(self) -> dict:
        inventory = {}
        for root in self.generated_directories:
            if root.resolve() != root:
                raise ValueError("generated header directory aliases another tree")
            if not root.exists():
                continue
            for directory, dirs, files in os.walk(root, followlinks=False):
                parent = Path(directory)
                for name in dirs:
                    if (parent / name).is_symlink():
                        raise ValueError("generated header directory contains a symlink")
                for name in files:
                    path = parent / name
                    if path.is_symlink():
                        raise ValueError("generated header inventory contains a symlink")
                    inventory[path] = self._record(path)
                    if len(inventory) > export.MAX_FILES:
                        raise ValueError("generated header inventory exceeds bound")
        return inventory

    def capture_configure(self, binary: Path, source: Path) -> None:
        if self.baseline is not None or not self.views or source != self.source:
            raise ValueError("portable configure authority is missing or repeated")
        if binary != self.build_root / "build/client" or binary.resolve() != binary:
            raise ValueError("portable binary differs from the caller's standalone build")
        self.binary = binary
        self._verify_authored()
        self.verify_host_tools()
        database = _read_regular(binary / "compile_commands.json")
        commands = json.loads(database, object_pairs_hook=export._unique_object)
        for record in commands:
            words = record.get("arguments") or shlex.split(record["command"])
            for value in _portable_include_operands(words):
                path = Path(value)
                path = path if path.is_absolute() else binary / path
                if path.is_relative_to(binary):
                    if path.resolve() != path:
                        raise ValueError("generated include directory is not canonical")
                    self.generated_directories.add(path)
        # The shader output is the only build-generated addition admitted later.
        self.generated_directories.add(binary / "src/include")
        self.baseline = {"cache": _read_regular(binary / "CMakeCache.txt"),
                         "database": database, "ninja": self._ninja(binary)}
        self.generated = self._generated_inventory()
        script = self.source / "tools/embed_gpu_shaders.py"
        manifest = self.source / "shaders/SHA256SUMS"
        if script not in self.authored or manifest not in self.authored:
            raise ValueError("shader generator or manifest is outside source closure")
        # Keep selected repository code outside the wrapper process. This is
        # the exact proof-bound CLI used by the admitted Ninja generator row.
        self.verify_host_tools()
        with tempfile.TemporaryDirectory(prefix=".atrinik-shader-proof-", dir=binary) as temporary:
            expected = Path(temporary) / "gpu_shader_data.h"
            try:
                subprocess.run([str(PORTABLE_GENERATOR_PYTHON), "-I", str(script), "--input", "/opt/atrinik-portable/shaders",
                                "--manifest", str(manifest), "--output", str(expected)],
                               check=True, timeout=30, stdin=subprocess.DEVNULL,
                               stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL,
                               env={"PATH": PORTABLE_PATH, "LC_ALL": "C"})
            except (OSError, subprocess.SubprocessError) as error:
                raise ValueError("source-bound shader proof subprocess failed") from error
            payload = _read_regular(expected, limit=512 * 1024 * 1024)
        self._verify_authored()
        self.verify_host_tools()
        self.expected_shader = {"sha256": hashlib.sha256(payload).hexdigest(), "size": len(payload)}
        if binary / "src/include/gpu_shader_data.h" in self.generated:
            raise ValueError("fresh configure unexpectedly retained a shader header")
        compile_map = {_portable_compile_output(record.get("arguments") or shlex.split(record["command"]), binary):
                       record.get("arguments") or shlex.split(record["command"]) for record in commands}
        self.target_outputs = set()
        _portable_ninja_commands(self.baseline["ninja"], binary, self.evidence, compile_map, self.source,
                                 compiled_outputs=self.target_outputs)
        validate_portable_build(binary, self.evidence, context=self, _configure_only=True)

    def capture_output(self, binary: Path) -> None:
        if self.output is not None or binary != self.binary or self.baseline is None:
            raise ValueError("portable output capture is missing or repeated")
        self.verify_host_tools()
        current = self._generated_inventory()
        shader = binary / "src/include/gpu_shader_data.h"
        if shader not in current or any(current[shader][key] != value for key, value in self.expected_shader.items()):
            raise ValueError("shader header differs from source-bound pinned cohort")
        if set(current) != set(self.generated) | {shader}:
            raise ValueError("build changed configured header inventory")
        for path, record in self.generated.items():
            if current[path] != record:
                raise ValueError("build changed configured header bytes or identity")
        self.generated = current
        self.output = self._record(binary / "atrinik")
        validate_portable_build(binary, self.evidence, context=self)
        self.verify_host_tools()

    def revalidate(self, binary: Path, *, configure_only: bool = False) -> None:
        if binary != self.binary or self.baseline is None or self.source is None:
            raise ValueError("same-operation portable receipt is missing")
        self.verify_host_tools()
        if self.baseline != {"cache": _read_regular(binary / "CMakeCache.txt"),
                             "database": _read_regular(binary / "compile_commands.json"),
                             "ninja": self._ninja(binary)}:
            raise ValueError("fresh configure baseline changed")
        self._verify_authored()
        if self._generated_inventory() != self.generated:
            raise ValueError("generated header inventory or bytes changed")
        if not configure_only and (self.output is None or self._record(binary / "atrinik") != self.output):
            raise ValueError("built executable receipt changed")
        self.verify_host_tools()

    def check_inputs(self, words: list[str], binary: Path, *, effective: bool = False,
                     expected_source: Path | None = None) -> None:
        forbidden = ("-include", "-imacros", "-include-pch", "-fmodule", "-ivfsoverlay", "-vfsoverlay",
                     "-iwithprefix", "-iframework", "-iprefix", "-working-directory", "-Wp,", "-Xpreprocessor",
                     "-index-header-map", "-fbuild-session", "-header-include-file")
        if any(word.startswith(forbidden) for word in words):
            raise ValueError("unsupported compiler input redirection")
        allowed_files = self.authored_inputs
        if effective:
            path = Path(words[-1]) if words else Path("")
            path = Path(os.path.normpath(str(path if path.is_absolute() else binary / path)))
            if not words or path not in allowed_files or path.resolve() != expected_source:
                raise ValueError("effective compiler source is unproven or changed")
        else:
            source = Path(words[words.index("-c") + 1])
            source = Path(os.path.normpath(str(source if source.is_absolute() else binary / source)))
            if source not in allowed_files:
                raise ValueError("translation unit is outside proven source views")
        system = {LLVM_ROOT + "/lib/clang/23/include", "/usr/local/include", "/usr/include",
                  "/usr/include/x86_64-linux-gnu", "/usr/include/libxml2", "/usr/include/c++/12",
                  "/usr/include/x86_64-linux-gnu/c++/12", "/usr/include/c++/12/backward",
                  "/usr/x86_64-linux-gnu/include"}
        allowed_dirs = self.authored_directories | self.generated_directories
        for value in _portable_include_operands(words, effective=effective):
            path = Path(value)
            path = Path(os.path.normpath(str(path if path.is_absolute() else binary / path)))
            if path in allowed_dirs or (str(path) in system and path.resolve() == path):
                continue
            # This absent built-in spelling comes only from the authenticated
            # Debian cc1 probe; it does not admit arbitrary files beneath /.
            probe = self.evidence.get("compile", {}).get("driver_output", "")
            builtin = any("-cc1" in args and any(args[index:index + 2] == ["-internal-externc-isystem", "/include"]
                          for index in range(len(args) - 1))
                          for line in probe.splitlines() if line.lstrip().startswith('"')
                          for args in [shlex.split(line)])
            if effective and value == "/include" and builtin and not path.exists():
                continue
            raise ValueError("compiler header search is outside admitted roots")


def _portable_include_operands(words: list[str], *, effective: bool = False) -> list[str]:
    result = []
    options = ("-internal-externc-isystem", "-internal-isystem", "-isystem", "-iquote", "-idirafter", "-I")
    for index, word in enumerate(words):
        for option in options:
            if word == option:
                if index + 1 >= len(words):
                    raise ValueError("missing compiler include directory")
                result.append(words[index + 1])
                break
            if word.startswith(option) and len(word) > len(option):
                if option.startswith("-internal") and not effective:
                    raise ValueError("explicit internal include override")
                result.append(word[len(option):])
                break
    return result


def _portable_command(words: list[str], cache: dict[str, str], *, link: bool, binary: Path | None = None) -> None:
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
        try:
            export.portable_library_paths(words, binary=binary, llvm_root=LLVM_ROOT, installed=True)
        except export.ExportError as error:
            raise WorkspaceError("portable linker search path changed") from error
        if not search_paths or search_paths[0] != LLVM_ROOT + "/lib":
            raise WorkspaceError("portable LLVM runtime search path changed")
    for option, expected in (("fuse-ld", "lld"), ("rtlib", "compiler-rt"), ("unwindlib", "libunwind")):
        family = [word for word in words if re.match(r"^-{1,2}" + option + r"(?:=|$)", word)]
        desired = ("-" if option == "fuse-ld" else "--") + option + "=" + expected
        if (link and (not family or any(word != desired for word in family))) or (not link and family):
            raise WorkspaceError("portable compile/link LLD/runtime contract changed")



def _portable_driver(words: list[str], evidence: dict, binary: Path, *, link: bool,
                     admitted_build_inputs: set[Path] | None = None, context: PortableBuildContext | None = None) -> set[Path] | None:
    """Bind effective driver tools and code generation to the verified cohort."""
    from .model import WorkspaceError
    try:
        output = export._tool_report([words[0], "-###", *words[1:]], cwd=binary, timeout=30)
        invocations = [shlex.split(line) for line in output.splitlines() if line.lstrip().startswith('"')]
        if not invocations or any(not args for args in invocations):
            raise ValueError("missing driver invocations")
        role = "cxx_compiler" if words[0] == LLVM_ROOT + "/bin/clang++" else "compiler"
        compiler = str(Path(evidence["tools"][role]["path"]).resolve())
        linker = evidence["tools"]["linker"]["path"]
        runtime = evidence["runtime"]["path"]
        cc1 = [args for args in invocations if len(args) > 1 and args[1] == "-cc1" and args[0] == compiler]
        links = [args for args in invocations if args[0] == linker]
        if (len(cc1) != (0 if link else 1) or len(links) != (1 if link else 0)
                or len(invocations) != 1):
            raise ValueError("external compiler, assembler, linker or extra tool selected")
        if cc1:
            args = cc1[0]
            if context is not None:
                source = Path(words[words.index("-c") + 1])
                source = source if source.is_absolute() else binary / source
                context.check_inputs(args, binary, effective=True, expected_source=source.resolve())
            if (args.count("-o") != 1
                    or any(arg.startswith("--output") or (arg.startswith("-o") and arg != "-o") for arg in args)
                    or _portable_build_path(args[args.index("-o") + 1], binary)
                    != _portable_compile_output(words, binary)):
                raise ValueError("effective compile output changed")
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
            if (words.count("-o") != 1 or args.count("-o") != 1
                    or any(arg.startswith("--output") or (arg.startswith("-o") and arg != "-o") for arg in args)
                    or _portable_build_path(args[args.index("-o") + 1], binary)
                    != _portable_build_path(words[words.index("-o") + 1], binary)):
                raise ValueError("effective link output changed")
            consumed = set()
            export.portable_link_inputs(args, binary=binary, llvm_root=LLVM_ROOT,
                                        admitted_build_inputs=admitted_build_inputs,
                                        observed_build_inputs=consumed)
            searches = export.portable_library_paths(args, binary=None if admitted_build_inputs is not None else binary,
                                                    llvm_root=LLVM_ROOT, installed=True)
            if (not searches or searches[0] != LLVM_ROOT + "/lib" or runtime not in args
                    or "-lunwind" not in args or args.count("-m") != 1
                    or args[args.index("-m") + 1] != "elf_x86_64"
                    or any(("libclang_rt.builtins" in arg and arg != runtime)
                           or ("libunwind" in arg and arg != evidence["unwinder"]["path"])
                           or arg in {"-lgcc", "-lgcc_s"} or re.search(r"libgcc[^/]*\.(?:a|so)", arg)
                           or arg.startswith(("--plugin", "-plugin", "--load-pass-plugin", "-load-pass-plugin", "--lto", "-lto")) for arg in args)):
                raise ValueError("effective linker or compiler runtime changed")
            return consumed
    except (OSError, ValueError, IndexError, KeyError, TypeError, export.ExportError, subprocess.SubprocessError) as error:
        raise WorkspaceError("portable effective compiler/linker contract changed") from error

def _portable_build_path(value: str, binary: Path) -> Path:
    """Resolve generated outputs and inputs only inside the canonical build tree."""
    path = Path(value)
    resolved = (path if path.is_absolute() else binary / path).resolve()
    if not resolved.is_relative_to(binary) or resolved == binary:
        raise ValueError("generated path escapes the portable build tree")
    return resolved


def _portable_compile_output(words: list[str], binary: Path) -> Path:
    if (words.count("-o") != 1 or words.count("-c") != 1
            or any(word.startswith("-o") and word != "-o" for word in words)
            or words.index("-o") + 1 >= len(words) or words.index("-c") + 1 >= len(words)):
        raise ValueError("invalid compile output or source")
    output = _portable_build_path(words[words.index("-o") + 1], binary)
    if output.suffix != ".o":
        raise ValueError("unexpected compile output")
    return output


def _portable_ninja_commands(report: str, binary: Path, evidence: dict,
                             compile_commands: dict[Path, list[str]], source: Path,
                             archive_members: dict[Path, set[Path]] | None = None,
                             compiled_outputs: set[Path] | None = None) -> tuple[list[str], set[Path]]:
    """Recognize the finite CMake recipes emitted by the pinned Classic source."""
    compiled = set()
    archives = set()
    link = None
    link_inputs = None
    generators = 0
    compilers = {LLVM_ROOT + "/bin/clang", LLVM_ROOT + "/bin/clang++"}
    for line in report.splitlines():
        if not line.strip():
            continue
        if link is not None:
            raise ValueError("unexpected command after final link")
        lexer = shlex.shlex(line, posix=True, punctuation_chars=";&|<>()")
        lexer.whitespace_split = True
        lexer.commenters = ""
        words = list(lexer)
        if any((word != "&&" and word and all(char in ";&|<>()" for char in word))
               or "$" in word or "`" in word or re.search(r"[*?\[]", word) for word in words):
            raise ValueError("unsupported Ninja shell command")
        segments = [[]]
        for word in words:
            if word == "&&":
                segments.append([])
            else:
                segments[-1].append(word)
        if any(not segment for segment in segments):
            raise ValueError("empty Ninja command")
        if len(segments) == 1 and words[0] in compilers and "-c" in words:
            output = _portable_compile_output(words, binary)
            compile_source = Path(words[words.index("-c") + 1])
            compile_source = (compile_source if compile_source.is_absolute() else binary / compile_source).resolve()
            if compile_source.is_relative_to(source.resolve()) and generators != 1:
                raise ValueError("client compilation precedes shader generator")
            normalized = list(words)
            option = words.index("-o")
            dependency = ["-MD", "-MT", words[option + 1], "-MF", words[option + 1] + ".d"]
            if option >= 5 and words[option - 5:option] == dependency:
                if words[option + 2:option + 3] != ["-c"]:
                    raise ValueError("unexpected dependency placement")
                normalized = words[:option - 5] + words[option:]
            if output in compiled or compile_commands.get(output) != normalized:
                raise ValueError("Ninja compiler differs from admitted compile database")
            compiled.add(output)
            continue
        if len(segments) == 5 and segments[0] == [":"] and segments[-1] == [":"]:
            remove, create, index = segments[1:4]
            ar = LLVM_ROOT + "/bin/llvm-ar"
            ranlib = LLVM_ROOT + "/bin/llvm-ranlib"
            if (len(remove) != 5 or remove[:4] != ["/usr/bin/cmake", "-E", "rm", "-f"]
                    or len(create) < 4 or create[:2] != [ar, "qc"]
                    or len(index) != 2 or index[0] != ranlib):
                raise ValueError("unexpected archive recipe")
            for role, tool in (("archiver", ar), ("ranlib", ranlib)):
                if Path(tool).resolve() != Path(evidence["tools"][role]["path"]).resolve():
                    raise ValueError("archive tool identity changed")
            archive = _portable_build_path(create[2], binary)
            if (archive.suffix != ".a" or archive in archives
                    or _portable_build_path(remove[4], binary) != archive
                    or _portable_build_path(index[1], binary) != archive):
                raise ValueError("archive output changed")
            members = [_portable_build_path(member, binary) for member in create[3:]]
            if len(set(members)) != len(members) or not set(members).issubset(compiled):
                raise ValueError("duplicate or missing earlier archive member producer")
            archives.add(archive)
            if archive_members is not None:
                archive_members[archive] = set(members)
            continue
        if len(segments) == 3 and segments[0] == [":"] and segments[-1] == [":"]:
            command = segments[1]
            if (link is not None or command[0] not in compilers or "-c" in command
                    or command.count("-o") != 1 or command.index("-o") + 1 >= len(command)
                    or _portable_build_path(command[command.index("-o") + 1], binary) != binary / "atrinik"):
                raise ValueError("unexpected final link recipe")
            link = command
            link_inputs = compiled | archives
            continue
        generator = [str(PORTABLE_GENERATOR_PYTHON), str(source / "tools/embed_gpu_shaders.py"),
                     "--input", "/opt/atrinik-portable/shaders", "--manifest", str(source / "shaders/SHA256SUMS"),
                     "--output", str(binary / "src/include/gpu_shader_data.h")]
        if segments == [["cd", str(binary)], generator] and generators == 0:
            generators += 1
            continue
        raise ValueError("unrecognized Ninja command recipe")
    if link is None or generators != 1:
        raise ValueError("missing final link, shader generator or compile database producer")
    if compiled_outputs is not None:
        compiled_outputs.update(compiled)
    return link, link_inputs


def validate_portable_build(binary: Path, evidence: dict, *, context: PortableBuildContext,
                            _configure_only: bool = False) -> bytes:
    """Check cached settings and generated compile/link commands before export."""
    from .model import WorkspaceError
    if not binary.is_absolute() or binary.resolve() != binary:
        raise WorkspaceError("portable build directory is not canonical")
    if not isinstance(context, PortableBuildContext):
        raise WorkspaceError("portable export requires caller source and build context")
    try:
        context.revalidate(binary, configure_only=_configure_only)
    except (OSError, ValueError, RuntimeError) as error:
        raise WorkspaceError("portable same-operation build receipt changed") from error
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
        if (source != context.source or values.get("CMAKE_HOME_DIRECTORY") != str(context.source)
                or identity.get("portable_host_tools") != json.loads(json.dumps(context.host_tools))
                or identity.get("portable_build_tools") != json.loads(json.dumps(context.build_tools))):
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
        compile_commands = {}
        for record in commands:
            words = record.get("arguments") or shlex.split(record["command"])
            if not isinstance(words, list) or not all(isinstance(word, str) for word in words):
                raise ValueError("invalid command")
            if record.get("directory") != str(binary):
                raise ValueError("compile working directory changed")
            output = _portable_compile_output(words, binary)
            compile_source = Path(words[words.index("-c") + 1])
            compile_source = (compile_source if compile_source.is_absolute() else binary / compile_source).resolve()
            declared_source = Path(record["file"])
            declared_source = (declared_source if declared_source.is_absolute() else binary / declared_source).resolve()
            if declared_source != compile_source:
                raise ValueError("compile database source operand changed")
            if output in compile_commands:
                raise ValueError("duplicate compile database output")
            compile_commands[output] = words
            context.check_inputs(words, binary)
            _portable_command(words, values, link=False)
            _portable_driver(words, evidence, binary, link=False, context=context)
        archives = {}
        observed_outputs = set()
        command, admitted = _portable_ninja_commands(context.baseline["ninja"], binary, evidence,
                                                     compile_commands, source, archives, observed_outputs)
        if observed_outputs != context.target_outputs:
            raise ValueError("fresh target compilation closure changed")
        _portable_command(command, values, link=True)
        if not _configure_only:
            consumed = _portable_driver(command, evidence, binary, link=True, admitted_build_inputs=admitted)
            pending = list(consumed)
            used_archives = set()
            used_objects = set()
            while pending:
                path = pending.pop()
                if path in archives:
                    used_archives.add(path)
                    pending.extend(archives[path])
                else:
                    used_objects.add(path)
            if used_objects != context.target_outputs or used_archives != set(archives):
                raise ValueError("compiled output or archive is not consumed by final link")
        context.verify_host_tools()
    except (OSError, ValueError, KeyError, TypeError, UnicodeError, subprocess.SubprocessError) as error:
        raise WorkspaceError("portable compile/link evidence is unavailable or invalid") from error
    return raw


def _verify_compiler_bytes(evidence: dict) -> None:
    """Rehash the selected tools and runtimes before using image metadata."""
    shared_unwinder = Path(evidence["unwinder"]["path"]).with_name("libunwind.so")
    if shared_unwinder.exists() or shared_unwinder.is_symlink():
        raise export.ExportError("portable-producer: unrecorded shared unwinder shadows static runtime")
    compiler = evidence["tools"]["compiler"]
    records = [*evidence["tools"].values(), evidence["runtime"], evidence["unwinder"]]
    if "compile" in evidence:
        stages = [shlex.split(line) for line in evidence["compile"]["driver_output"].splitlines()
                  if line.lstrip().startswith('"')]
        records.extend({"path": args[0], "sha256": compiler["sha256"]}
                       for args in stages if len(args) > 1 and args[1] == "-cc1")
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
    _capture_command_shell()
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
    for expected in export.portable_build_tools(documents["installed.json"].get("build_tools")).values():
        PortableBuildContext._build_tool_record(expected)
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
        context = PortableBuildContext(selected, proofs, generations, documents["compiler-evidence.json"],
                                       build_tools=documents["installed.json"]["build_tools"])
        targets = workspace._expand_build_target("client", profile_name)
        build_root = workspace._build_resolved("client", profile_name, False,
                    targets, selected, build_services={"client"},
                    generate_region_maps=False, portable=True, portable_context=context)
        with workspace._profile_build_lock(build_root, profile_name):
            # A concurrent rebuild between build return and lock acquisition must
            # not silently replace the pinned portable configure contract.
            binary_directory = context.binary
            if binary_directory is None or workspace._classic_binary_directory(build_root, "client") != binary_directory:
                raise WorkspaceError("portable build graph coordinate differs from same-operation receipt")
            cache = validate_portable_build(binary_directory, documents["compiler-evidence.json"], context=context)
            sound_root, sound_record = workspace._prepare_sound(build_root, selected, profile_name)
            if sound_record["source_tree"] != generations["sound"]["source_tree"]:
                raise WorkspaceError("portable sound release must match the selected sound source tree")
            sources = {"atrinik/" + name + "@main": state["head"] for name, state in states.items()}
            sources["atrinik/devcontainer@main"] = PRODUCER_COMMIT
            with Publication(destination, sources) as publication:
                context.verify_host_tools()
                runtime = add_runtime(publication, documents, binary_directory / "atrinik")
                context.verify_host_tools()
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
                            "host_tools": context.host_tools,
                            "build_tools": context.build_tools,
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
                    validate_portable_build(binary_directory, documents["compiler-evidence.json"], context=context)
                result = publication.publish(revalidate)
                return {**result, "image": IMAGE, "platform_manifest": PLATFORM_MANIFEST,
                        "runtime": runtime, "legal": legal}
