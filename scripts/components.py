#!/usr/bin/env python3
"""Fetch and verify Atrinik release artifacts pinned by components.lock.json."""

from __future__ import annotations

import argparse
import hashlib
import json
import os
from pathlib import Path, PurePosixPath
import re
import shutil
import stat
import tarfile
import tempfile
from typing import BinaryIO, Iterable
import urllib.error
import urllib.parse
import urllib.request


LOCK_SCHEMA_VERSION = 1
MARKER_NAME = ".atrinik-dependency.json"
MAX_ARCHIVE_BYTES = 2 * 1024 * 1024 * 1024
MAX_EXPANDED_BYTES = 4 * 1024 * 1024 * 1024
MAX_FILE_BYTES = 512 * 1024 * 1024
MAX_MEMBERS = 100_000
MAX_DEPENDENCIES = 12
COPY_CHUNK_BYTES = 1024 * 1024


class DependencyError(RuntimeError):
    """A dependency lock or artifact failed validation."""


def _reject_duplicate_keys(pairs: Iterable[tuple[str, object]]) -> dict[str, object]:
    result: dict[str, object] = {}
    for key, value in pairs:
        if key in result:
            raise DependencyError(f"duplicate JSON key: {key}")
        result[key] = value
    return result


def _require_keys(value: dict[str, object], expected: set[str], context: str) -> None:
    actual = set(value)
    if actual != expected:
        missing = sorted(expected - actual)
        extra = sorted(actual - expected)
        details = []
        if missing:
            details.append(f"missing {', '.join(missing)}")
        if extra:
            details.append(f"unexpected {', '.join(extra)}")
        raise DependencyError(f"{context}: {'; '.join(details)}")


def _validate_text(value: object, field: str) -> str:
    if not isinstance(value, str) or not value or value != value.strip():
        raise DependencyError(f"{field} must be a non-empty trimmed string")
    return value


def load_lock(path: Path, *, allow_file_urls: bool = False) -> list[dict[str, object]]:
    try:
        with path.open(encoding="utf-8") as stream:
            root = json.load(stream, object_pairs_hook=_reject_duplicate_keys)
    except (OSError, json.JSONDecodeError) as error:
        raise DependencyError(f"cannot read {path}: {error}") from error

    if not isinstance(root, dict):
        raise DependencyError("lock root must be an object")
    _require_keys(root, {"schema_version", "components"}, "lock root")
    if root["schema_version"] != LOCK_SCHEMA_VERSION:
        raise DependencyError(
            f"unsupported lock schema version: {root['schema_version']}"
        )
    dependencies = root["components"]
    if not isinstance(dependencies, list) or not dependencies:
        raise DependencyError("components must be a non-empty array")
    if len(dependencies) > MAX_DEPENDENCIES:
        raise DependencyError(
            f"components cannot contain more than {MAX_DEPENDENCIES} entries"
        )

    expected = {
        "name",
        "repository",
        "tag",
        "commit",
        "url",
        "sha256",
        "destination",
        "strip_components",
    }
    names: set[str] = set()
    destinations: set[str] = set()
    validated: list[dict[str, object]] = []
    for index, item in enumerate(dependencies):
        context = f"dependency {index}"
        if not isinstance(item, dict):
            raise DependencyError(f"{context} must be an object")
        _require_keys(item, expected, context)
        values = {
            key: _validate_text(item[key], f"{context}.{key}")
            for key in expected - {"strip_components"}
        }

        name = values["name"]
        if not all(char.islower() or char.isdigit() or char == "-" for char in name):
            raise DependencyError(f"{context}.name must use lowercase kebab-case")
        if name in names:
            raise DependencyError(f"duplicate dependency name: {name}")
        names.add(name)

        repository = values["repository"]
        if not re.fullmatch(r"atrinik/[a-z0-9][a-z0-9._-]*", repository):
            raise DependencyError(
                f"{context}.repository must identify an atrinik organization repository"
            )
        tag = values["tag"]
        if not re.fullmatch(r"v[0-9]+\.[0-9]+\.[0-9]+", tag):
            raise DependencyError(f"{context}.tag must be an immutable semantic-version tag")
        commit = values["commit"]
        if len(commit) != 40 or any(char not in "0123456789abcdef" for char in commit):
            raise DependencyError(f"{context}.commit must be a full lowercase Git SHA")
        digest = values["sha256"]
        if len(digest) != 64 or any(char not in "0123456789abcdef" for char in digest):
            raise DependencyError(f"{context}.sha256 must be a lowercase SHA-256")

        parsed_url = urllib.parse.urlparse(values["url"])
        try:
            parsed_port = parsed_url.port
        except ValueError as error:
            raise DependencyError(f"{context}.url has an invalid port") from error
        allowed_schemes = {"https"}
        if allow_file_urls:
            allowed_schemes.add("file")
        if (
            parsed_url.scheme not in allowed_schemes
            or not parsed_url.path.endswith(".tar.gz")
            or parsed_url.query
            or parsed_url.fragment
        ):
            raise DependencyError(f"{context}.url must identify an allowed .tar.gz asset")
        if parsed_url.scheme == "https":
            expected_prefix = f"/{repository}/releases/download/{tag}/"
            if (
                parsed_url.hostname != "github.com"
                or parsed_port is not None
                or parsed_url.username is not None
                or parsed_url.password is not None
                or not parsed_url.path.startswith(expected_prefix)
            ):
                raise DependencyError(
                    f"{context}.url must match its GitHub repository and tag"
                )

        destination = PurePosixPath(values["destination"])
        normalized_destination = destination.as_posix()
        if (
            destination.is_absolute()
            or len(destination.parts) < 2
            or any(part in {"", ".", ".."} for part in destination.parts)
            or destination.parts[0].startswith(".")
            or values["destination"] != normalized_destination
            or "\\" in values["destination"]
            or ":" in values["destination"]
        ):
            raise DependencyError(
                f"{context}.destination must be a safe canonical nested path"
            )
        destination_key = normalized_destination.casefold()
        if destination_key in destinations:
            raise DependencyError(f"duplicate dependency destination: {normalized_destination}")
        destinations.add(destination_key)

        strip_components = item["strip_components"]
        if not isinstance(strip_components, int) or not 1 <= strip_components <= 8:
            raise DependencyError(f"{context}.strip_components must be between 1 and 8")

        validated.append({**values, "strip_components": strip_components})
    return validated


def sha256_file(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(COPY_CHUNK_BYTES), b""):
            digest.update(chunk)
    return digest.hexdigest()


def _download(dependency: dict[str, object], cache_dir: Path) -> Path:
    cache_dir.mkdir(parents=True, exist_ok=True)
    expected = str(dependency["sha256"])
    archive = cache_dir / f"{dependency['name']}-{expected}.tar.gz"
    if archive.exists():
        if (
            archive.is_file()
            and archive.stat().st_size <= MAX_ARCHIVE_BYTES
            and sha256_file(archive) == expected
        ):
            return archive
        archive.unlink()

    request = urllib.request.Request(
        str(dependency["url"]),
        headers={"User-Agent": "Atrinik dependency fetcher/1"},
    )
    descriptor, temporary_name = tempfile.mkstemp(
        prefix=f".{archive.name}.part-", dir=cache_dir
    )
    os.close(descriptor)
    temporary = Path(temporary_name)
    try:
        with (
            urllib.request.urlopen(request, timeout=60) as response,
            temporary.open("wb") as output,
        ):
            requested_scheme = urllib.parse.urlparse(str(dependency["url"])).scheme
            response_scheme = urllib.parse.urlparse(response.geturl()).scheme
            if requested_scheme not in {"https", "file"} or response_scheme != requested_scheme:
                raise DependencyError(f"{dependency['name']}: download changed URL scheme")
            content_length = response.headers.get("Content-Length")
            if content_length is not None:
                try:
                    declared_size = int(content_length)
                except ValueError as error:
                    raise DependencyError(
                        f"{dependency['name']}: invalid Content-Length"
                    ) from error
                if declared_size < 0 or declared_size > MAX_ARCHIVE_BYTES:
                    raise DependencyError(f"{dependency['name']}: archive exceeds size limit")
            total = 0
            while True:
                chunk = response.read(COPY_CHUNK_BYTES)
                if not chunk:
                    break
                total += len(chunk)
                if total > MAX_ARCHIVE_BYTES:
                    raise DependencyError(f"{dependency['name']}: archive exceeds size limit")
                output.write(chunk)
        if sha256_file(temporary) != expected:
            raise DependencyError(f"{dependency['name']}: downloaded SHA-256 does not match lock")
        temporary.replace(archive)
    except Exception:
        temporary.unlink(missing_ok=True)
        raise
    return archive


def _stripped_path(name: str, count: int) -> PurePosixPath | None:
    if not name or "\0" in name or "\\" in name or ":" in name:
        raise DependencyError(f"unsafe archive member path: {name}")
    path = PurePosixPath(name)
    if path.is_absolute() or any(part in {"", ".", ".."} for part in path.parts):
        raise DependencyError(f"unsafe archive member path: {name}")
    if len(path.parts) <= count:
        return None
    return PurePosixPath(*path.parts[count:])


def _copy_member(source: BinaryIO, destination: Path, expected_size: int) -> None:
    written = 0
    with destination.open("xb") as output:
        while written < expected_size:
            chunk = source.read(min(COPY_CHUNK_BYTES, expected_size - written))
            if not chunk:
                raise DependencyError(f"truncated archive member: {destination.name}")
            output.write(chunk)
            written += len(chunk)
        if source.read(1):
            raise DependencyError(f"archive member exceeds declared size: {destination.name}")


def extract_archive(archive_path: Path, destination: Path, strip_components: int) -> None:
    seen: set[str] = set()
    expanded_bytes = 0
    file_count = 0
    member_count = 0
    try:
        archive = tarfile.open(archive_path, mode="r:gz")
    except (OSError, tarfile.TarError) as error:
        raise DependencyError(f"cannot open {archive_path}: {error}") from error

    with archive:
        for member in archive:
            member_count += 1
            if member_count > MAX_MEMBERS:
                raise DependencyError("archive has too many members")
            relative = _stripped_path(member.name, strip_components)
            if relative is None:
                continue
            key = relative.as_posix().casefold()
            if key in seen:
                raise DependencyError(f"duplicate archive output path: {relative}")
            seen.add(key)
            output_path = destination.joinpath(*relative.parts)
            if member.isdir():
                output_path.mkdir(parents=True, exist_ok=True)
                continue
            if not member.isfile():
                raise DependencyError(f"unsupported archive member type: {member.name}")
            file_count += 1
            if member.size < 0 or member.size > MAX_FILE_BYTES:
                raise DependencyError(f"archive member exceeds size limit: {member.name}")
            expanded_bytes += member.size
            if expanded_bytes > MAX_EXPANDED_BYTES:
                raise DependencyError("archive exceeds expanded size limit")
            output_path.parent.mkdir(parents=True, exist_ok=True)
            source = archive.extractfile(member)
            if source is None:
                raise DependencyError(f"cannot read archive member: {member.name}")
            with source:
                _copy_member(source, output_path, member.size)
            mode = stat.S_IMODE(member.mode) & 0o755
            output_path.chmod(mode if mode else 0o644)
    if file_count == 0:
        raise DependencyError("archive contains no files after stripping its prefix")


def marker_for(dependency: dict[str, object]) -> dict[str, object]:
    return {
        "schema_version": LOCK_SCHEMA_VERSION,
        "name": dependency["name"],
        "repository": dependency["repository"],
        "tag": dependency["tag"],
        "commit": dependency["commit"],
        "sha256": dependency["sha256"],
    }


def read_marker(destination: Path) -> dict[str, object] | None:
    marker = destination / MARKER_NAME
    try:
        with marker.open(encoding="utf-8") as stream:
            value = json.load(stream, object_pairs_hook=_reject_duplicate_keys)
    except FileNotFoundError:
        return None
    except (OSError, json.JSONDecodeError, DependencyError) as error:
        raise DependencyError(f"invalid managed dependency marker at {marker}: {error}") from error
    if not isinstance(value, dict):
        raise DependencyError(
            f"invalid managed dependency marker at {marker}: root must be an object"
        )
    try:
        _require_keys(
            value,
            {"schema_version", "name", "repository", "tag", "commit", "sha256"},
            "dependency marker",
        )
        if value["schema_version"] != LOCK_SCHEMA_VERSION:
            raise DependencyError(
                f"unsupported marker schema version: {value['schema_version']}"
            )
        for field in ("name", "repository", "tag", "commit", "sha256"):
            _validate_text(value[field], f"dependency marker.{field}")
    except DependencyError as error:
        raise DependencyError(f"invalid managed dependency marker at {marker}: {error}") from error
    return value


def install_dependency(
    root: Path,
    cache_dir: Path,
    dependency: dict[str, object],
    *,
    refresh: bool = False,
) -> str:
    root = root.resolve(strict=True)
    destination = (root / str(dependency["destination"])).resolve(strict=False)
    try:
        destination.relative_to(root)
    except ValueError as error:
        raise DependencyError(f"destination escapes repository: {destination}") from error
    expected_marker = marker_for(dependency)
    existing_marker = read_marker(destination) if destination.exists() else None
    if destination.exists() and existing_marker == expected_marker and not refresh:
        return "current"
    if destination.exists() and existing_marker is None:
        raise DependencyError(f"refusing to replace unmanaged destination: {destination}")

    archive = _download(dependency, cache_dir)
    destination.parent.mkdir(parents=True, exist_ok=True)
    staging = Path(
        tempfile.mkdtemp(
            prefix=f".{dependency['name']}-staging-", dir=destination.parent
        )
    )
    backup: Path | None = None
    try:
        extract_archive(archive, staging, int(dependency["strip_components"]))
        marker = staging / MARKER_NAME
        marker.write_text(
            json.dumps(expected_marker, indent=2, sort_keys=True) + "\n",
            encoding="utf-8",
        )
        if destination.exists():
            backup = Path(
                tempfile.mkdtemp(
                    prefix=f".{dependency['name']}-backup-", dir=destination.parent
                )
            )
            backup.rmdir()
            destination.replace(backup)
        staging.replace(destination)
        if backup is not None:
            shutil.rmtree(backup)
    except Exception:
        shutil.rmtree(staging, ignore_errors=True)
        if backup is not None and backup.exists() and not destination.exists():
            backup.replace(destination)
        raise
    return "installed"


def verify_dependency(root: Path, dependency: dict[str, object]) -> None:
    destination = root / str(dependency["destination"])
    if not destination.is_dir():
        raise DependencyError(f"missing dependency {dependency['name']} at {destination}")
    if read_marker(destination) != marker_for(dependency):
        raise DependencyError(f"dependency {dependency['name']} does not match the lock")


def _consumer_pins(path: Path) -> list[dict[str, str]]:
    try:
        with path.open(encoding="utf-8") as stream:
            root = json.load(stream, object_pairs_hook=_reject_duplicate_keys)
    except (OSError, json.JSONDecodeError, DependencyError) as error:
        raise DependencyError(f"cannot read consumer lock {path}: {error}") from error
    if not isinstance(root, dict) or root.get("schema_version") != LOCK_SCHEMA_VERSION:
        raise DependencyError(f"invalid consumer lock schema at {path}")
    raw_dependencies = root.get("dependencies")
    if isinstance(raw_dependencies, dict):
        items = list(raw_dependencies.values())
    elif isinstance(raw_dependencies, list):
        items = raw_dependencies
    else:
        raise DependencyError(f"invalid dependencies collection in {path}")
    if not items:
        raise DependencyError(f"consumer lock has no dependencies at {path}")

    fields = ("repository", "tag", "commit", "url", "sha256")
    pins: list[dict[str, str]] = []
    for index, item in enumerate(items):
        if not isinstance(item, dict):
            raise DependencyError(f"invalid consumer dependency {index} in {path}")
        pins.append(
            {
                field: _validate_text(
                    item.get(field), f"consumer dependency {index}.{field}"
                )
                for field in fields
            }
        )
    return pins


def _is_runtime_asset(url: str) -> bool:
    return "-runtime." in PurePosixPath(urllib.parse.urlparse(url).path).name


def verify_consumer_locks(
    root: Path, dependencies: list[dict[str, object]]
) -> None:
    by_name = {str(dependency["name"]): dependency for dependency in dependencies}
    required_components = {
        "client": {"protocol", "libatrinik", "sound"},
        "server": {"protocol", "libatrinik", "content", "resources"},
    }
    for component_name in ("client", "server"):
        if component_name not in by_name:
            raise DependencyError(f"integration lock is missing {component_name}")
        component_root = root / str(by_name[component_name]["destination"])
        declared: set[tuple[str, bool]] = set()
        for relative_lock in (
            Path("dependencies.lock.json"),
            Path("cmake/dependencies.lock.json"),
        ):
            path = component_root / relative_lock
            for pin in _consumer_pins(path):
                identity = (pin["repository"], _is_runtime_asset(pin["url"]))
                declared.add(identity)
                candidates = [
                    dependency
                    for dependency in dependencies
                    if dependency["repository"] == pin["repository"]
                    and _is_runtime_asset(str(dependency["url"]))
                    == _is_runtime_asset(pin["url"])
                ]
                if len(candidates) != 1:
                    raise DependencyError(
                        f"{path}: {pin['repository']} does not resolve to one "
                        "integration dependency"
                    )
                candidate = candidates[0]
                for field in ("tag", "commit", "url", "sha256"):
                    if candidate[field] != pin[field]:
                        raise DependencyError(
                            f"{path}: {pin['repository']} {field} differs from "
                            "the integration lock"
                        )
        for required_name in required_components[component_name]:
            if required_name not in by_name:
                raise DependencyError(
                    f"integration lock is missing required {component_name} "
                    f"dependency {required_name}"
                )
            required = by_name[required_name]
            identity = (
                str(required["repository"]),
                _is_runtime_asset(str(required["url"])),
            )
            if identity not in declared:
                raise DependencyError(
                    f"{component_name} consumer locks do not declare required "
                    f"dependency {required_name}"
                )


def sync_dependencies(
    root: Path,
    cache_dir: Path,
    dependencies: list[dict[str, object]],
    *,
    refresh: bool = False,
) -> list[tuple[str, str]]:
    ordered = sorted(
        dependencies,
        key=lambda item: len(PurePosixPath(str(item["destination"])).parts),
    )
    reinstalled: list[PurePosixPath] = []
    statuses: list[tuple[str, str]] = []
    for dependency in ordered:
        destination = PurePosixPath(str(dependency["destination"]))
        parent_changed = any(parent in destination.parents for parent in reinstalled)
        status = install_dependency(
            root,
            cache_dir,
            dependency,
            refresh=refresh or parent_changed,
        )
        if status == "installed":
            reinstalled.append(destination)
        statuses.append((str(dependency["name"]), status))
    return statuses


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("command", choices=("validate", "sync", "verify", "list"))
    parser.add_argument("--root", type=Path, default=Path(__file__).resolve().parents[1])
    parser.add_argument("--lock", type=Path)
    parser.add_argument("--cache", type=Path)
    parser.add_argument(
        "--refresh",
        action="store_true",
        help="reinstall even when the marker is current",
    )
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    root = args.root.resolve(strict=True)
    lock_path = args.lock or root / "components.lock.json"
    cache_dir = args.cache or root / "build" / "cache" / "downloads"
    try:
        dependencies = load_lock(lock_path)
        if args.command == "list":
            for dependency in dependencies:
                print(f"{dependency['name']}\t{dependency['tag']}\t{dependency['destination']}")
        elif args.command == "sync":
            for name, status in sync_dependencies(
                root, cache_dir, dependencies, refresh=args.refresh
            ):
                print(f"{name}: {status}")
        elif args.command == "verify":
            for dependency in dependencies:
                verify_dependency(root, dependency)
                print(f"{dependency['name']}: verified")
            verify_consumer_locks(root, dependencies)
            print("consumer dependency locks: verified")
        else:
            print(f"{lock_path}: valid ({len(dependencies)} dependencies)")
    except (DependencyError, OSError, urllib.error.URLError) as error:
        print(f"component error: {error}", file=os.sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
