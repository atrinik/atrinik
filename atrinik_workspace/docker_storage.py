"""Collision-safe Docker volume names and package mount specifications."""

from __future__ import annotations

from dataclasses import dataclass
import hashlib
import os
from pathlib import Path
import re

from .model import WorkspaceError


VOLUME_NAMESPACE_ENV = "ATRINIK_DOCKER_VOLUME_NAMESPACE"
_NAMESPACE_PATTERN = re.compile(r"^[a-z0-9][a-z0-9_.-]{0,62}$")
_PURPOSE_PATTERN = re.compile(r"^[a-z0-9][a-z0-9-]{0,62}$")
_MAX_DOCKER_VOLUME_NAME_LENGTH = 255


@dataclass(frozen=True)
class DockerVolumeMount:
    """One writable named volume attached to a fixed container target."""

    purpose: str
    name: str
    target: str

    @property
    def docker_spec(self) -> str:
        return f"type=volume,source={self.name},target={self.target},volume-nocopy"


WINDOWS_PACKAGE_VOLUME_TARGETS = (
    ("windows-client-build", "/workspace/client/build"),
    ("windows-server-build", "/workspace/server/build"),
    ("windows-compiler-cache", "/workspace/.ccache"),
    ("windows-dependency-downloads", "/workspace/.dependency-downloads"),
)


def _configured_namespace() -> str | None:
    value = os.environ.get(VOLUME_NAMESPACE_ENV)
    if value is None:
        return None
    if _NAMESPACE_PATTERN.fullmatch(value) is None:
        raise WorkspaceError(
            f"{VOLUME_NAMESPACE_ENV} must be a lowercase Docker-safe namespace"
        )
    return value


def volume_namespace(repository: Path | str) -> str:
    """Return the configured or path-derived collision-resistant namespace."""

    configured = _configured_namespace()
    if configured is not None:
        return configured
    resolved = Path(repository).expanduser().resolve(strict=False)
    digest = hashlib.sha256(os.fsencode(str(resolved))).hexdigest()[:16]
    return f"path-{digest}"


def volume_name(repository: Path | str, purpose: str) -> str:
    """Return a bounded, collision-safe Docker volume name."""

    if _PURPOSE_PATTERN.fullmatch(purpose) is None:
        raise WorkspaceError(f"invalid Docker volume purpose: {purpose!r}")
    name = f"atrinik-{volume_namespace(repository)}-{purpose}"
    if len(name) > _MAX_DOCKER_VOLUME_NAME_LENGTH:
        raise WorkspaceError("Docker volume name exceeds the Docker limit")
    return name


def windows_package_volume_mounts(
    repository: Path | str,
) -> tuple[DockerVolumeMount, ...]:
    """Describe isolated writable volumes used by the Windows package build."""

    return tuple(
        DockerVolumeMount(
            purpose=purpose,
            name=volume_name(repository, purpose),
            target=target,
        )
        for purpose, target in WINDOWS_PACKAGE_VOLUME_TARGETS
    )
