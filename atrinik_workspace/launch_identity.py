from __future__ import annotations

from .model import WorkspaceError, validate_name


CLIENT_LAUNCH_LABEL_ENV = "ATRINIK_LAUNCH_LABEL"
CLIENT_LAUNCH_LABEL_MAX_SIZE = 96


def client_launch_label(profile: str, topology: str | None = None) -> str:
    validate_name(profile, "profile name")
    if topology is None:
        label = f"profile {profile} (direct run)"
    else:
        validate_name(topology, "topology name")
        label = f"topology {topology} - profile {profile}"
    if len(label.encode("ascii")) > CLIENT_LAUNCH_LABEL_MAX_SIZE:
        raise WorkspaceError(
            f"client launch label exceeds {CLIENT_LAUNCH_LABEL_MAX_SIZE} bytes"
        )
    return label
