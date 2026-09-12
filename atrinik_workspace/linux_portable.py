"""Produce a movable Classic client from leased sources and the pinned toolchain.

The portable image is a build input, never delivery-coordinator authority.
Publication verifies payload bytes and source/build closure before one no-replace
rename. Interrupted private staging remains available for diagnosis.
"""
from __future__ import annotations

from contextlib import ExitStack
import hashlib
import json
import os
from pathlib import Path
import re
import secrets
import stat
from typing import Callable

from . import linux_export as export

IMAGE = "ghcr.io/atrinik/classic-portable-build@sha256:df72e2ece5edeaee584a1b8eb30e523c6154a0adae7a1fea5e954ed6bc9dbae1"
PLATFORM_MANIFEST = "sha256:40412cc14527deb333273527681fb0639efef90e68b2ca508844ada38da9f2a6"
PRODUCER_COMMIT = "aa7e944ec3afcf2eaec434b164618680d514a77b"
CONSUMER_COMMIT = "4998131ad2ae4c9680685fd87e2d85de1dc15fd9"
METADATA_HASHES = {
    "contract.json": "d450cc76acd7148b80f3b80198d3c11590727df2d70c45db28d740b8e86579db",
    "debian-sources.json": "fd83eb65839b76f692b26cb8b333c9baa0f6ce7379e3d6477351ea501342aec4",
    "installed.json": "ec878ca93bc19bb2b32769abf4170125c22e09e0fdd42fda5fe3c7735033746a",
    "runtime-abi.json": "66ca8945657990b58fde5ee8c1ea44da1444779ece78738b09640bca65dba2b6",
    "runtime-sources.json": "ac034c398016886d90d1f8d9601a61040bd760b9c8965c2f258818bf7fa3bf3d",
    "shader-generation.json": "24e04e4cccb84a1e374485c27d1fe250ed2aaf3033a05fa1219b13f77d406bf0",
}
# Complete final OCI overlay, independently reconstructed from every layer of
# PLATFORM_MANIFEST. Canonical JSON is sorted with compact separators; source
# records contain relative path -> sha256/size/executable. This pins omitted
# archives/recipes/notices as well as bytes, without a caller-supplied override.
PRODUCER_FILES_SHA256 = 'da6ee7e46cfb5b254a4811adf65a6056719114a8755f65db3605c2802c9cfc52'
DEBIAN_NOTICES_SHA256 = '6670f1afe9d1face038ae959386c3f264a4b3f05328c493579ea359a8cd2f4e5'
COMMON_LICENSES_SHA256 = 'cfe52936825faa1cd66a4b17ed41051f5c777c5035c8206eaad4db5b730f2bfd'
COMMON_LICENSE_NAMES = ('Apache-2.0', 'Artistic', 'BSD', 'CC0-1.0', 'GFDL-1.2', 'GFDL-1.3', 'GPL-1', 'GPL-2', 'GPL-3', 'LGPL-2', 'LGPL-2.1', 'LGPL-3', 'MPL-1.1', 'MPL-2.0')
# Use the host's glibc/loader so its graphics-driver modules can load against
# their matching system libc. Application libraries and plugins are bundled.
HOST_GLIBC = frozenset({"libc.so.6", "libm.so.6", "libpthread.so.0", "libdl.so.2",
                        "librt.so.1", "libresolv.so.2", "libutil.so.1",
                        "ld-linux-x86-64.so.2"})


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


def installed_metadata() -> tuple[dict[str, bytes], dict[str, dict]]:
    root = Path("/opt/atrinik-portable")
    raw = {name: _read_regular(root / name) for name in METADATA_HASHES}
    export.portable_metadata_report(raw, expected_hashes=METADATA_HASHES,
                                   immutable_image=IMAGE, runnable_manifest=PLATFORM_MANIFEST,
                                   consumer_commit=CONSUMER_COMMIT)
    return raw, {name: json.loads(data) for name, data in raw.items()}


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
            facts = export.inspect_elf(descriptor)
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
            "dynamic_sonames": dynamic, "symbols": symbols,
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
    return {"debian_source_packages": len(source_packages), "debian_archives": len(archives),
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
            cache = (binary_directory / "CMakeCache.txt").read_text()
            for expected in ("CMAKE_SKIP_RPATH:BOOL=ON", "CMAKE_BUILD_TYPE:STRING=Release",
                             "CMAKE_C_FLAGS:STRING=-O2 -march=x86-64 -mtune=generic"):
                if expected not in cache.splitlines():
                    raise WorkspaceError("portable CMake configuration changed: " + expected.split(":", 1)[0])
            sound_root, sound_record = workspace._prepare_sound(build_root, selected, profile_name)
            if sound_record["source_tree"] != generations["sound"]["source_tree"]:
                raise WorkspaceError("portable sound release must match the selected sound source tree")
            sources = {"atrinik/" + name + "@main": state["head"] for name, state in states.items()}
            sources["atrinik/devcontainer@main"] = PRODUCER_COMMIT
            with Publication(destination, sources) as publication:
                runtime = add_runtime(publication, documents, binary_directory / "atrinik")
                legal = add_corresponding_sources(publication, documents)
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
                    if (binary_directory / "CMakeCache.txt").read_text() != cache:
                        raise WorkspaceError("portable build configuration changed before publication")
                result = publication.publish(revalidate)
                return {**result, "image": IMAGE, "platform_manifest": PLATFORM_MANIFEST,
                        "runtime": runtime, "legal": legal}
