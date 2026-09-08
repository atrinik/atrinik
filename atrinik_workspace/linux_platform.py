"""Read-only Linux prerequisites and opt-in desktop container capabilities.

This module does not grant delivery authority or launch Docker. The standalone
coordinator probe owns execution authority.
"""
from __future__ import annotations

import argparse
import json
import os
from pathlib import Path
import re
import shutil
import stat
import subprocess
from typing import Mapping


class PlatformError(ValueError):
    """An unsupported or unsafe capability request."""


def _mount(source: Path, target: str) -> list[str]:
    if not source.is_absolute() or ".." in source.parts or any(c in str(source) for c in ",\n\r\x00"):
        raise PlatformError("desktop-mount-path: use an absolute unambiguous path")
    return ["--mount", f"type=bind,source={source},target={target},readonly"]


def _endpoint(path: Path, uid: int, *, socket: bool) -> None:
    try:
        info = path.lstat()
        expected = stat.S_ISSOCK if socket else stat.S_ISREG
        if not expected(info.st_mode) or info.st_uid != uid:
            raise PlatformError("desktop-endpoint-owner-or-type")
        if not socket and info.st_mode & 0o077:
            raise PlatformError("desktop-authority-mode: authority files must be private")
        for parent in path.parents:
            info = parent.lstat()
            if not stat.S_ISDIR(info.st_mode) or info.st_uid not in (0, uid):
                raise PlatformError("desktop-endpoint-ancestor")
            if info.st_mode & 0o022 and not (info.st_uid == 0 and info.st_mode & stat.S_ISVTX):
                raise PlatformError("desktop-endpoint-ancestor")
    except OSError:
        raise PlatformError("desktop-endpoint-missing: inspect the active session") from None


def desktop_options(
    *, display: str, environment: Mapping[str, str], uid: int,
    gpu: str, audio: bool = False, render_devices: tuple[Path, ...] = (),
) -> list[str]:
    """Return explicit desktop additions for a container mapped to the host UID.

    Hardware gameplay still requires independent selected-renderer evidence.
    No X-server ACL is changed, and only the selected session sockets are shared.
    """
    result: list[str] = []
    if display == "x11":
        value = environment.get("DISPLAY", "")
        match = re.fullmatch(r":([0-9]+)(?:\.[0-9]+)?", value)
        if not match:
            raise PlatformError("x11-display: use a local :N display")
        source = Path(environment.get("XAUTHORITY", ""))
        if not source.is_absolute():
            raise PlatformError("x11-authority: supply the actual private XAUTHORITY file")
        _endpoint(source, uid, socket=False)
        endpoint = Path("/tmp/.X11-unix") / ("X" + match[1])
        # X server sockets may belong to root; the cookie remains user-private.
        try:
            socket_uid = endpoint.lstat().st_uid
        except OSError:
            raise PlatformError("x11-socket-missing: inspect the local X server") from None
        if socket_uid not in (0, uid):
            raise PlatformError("x11-socket-owner: select your local X server")
        _endpoint(endpoint, socket_uid, socket=True)
        result += _mount(endpoint, str(endpoint))
        result += _mount(source, "/run/atrinik-xauthority")
        result += ["--env", f"DISPLAY={value}", "--env", "XAUTHORITY=/run/atrinik-xauthority",
                   "--env", "SDL_VIDEODRIVER=x11"]
    elif display == "wayland":
        runtime = Path(environment.get("XDG_RUNTIME_DIR", ""))
        name = environment.get("WAYLAND_DISPLAY", "")
        if not runtime.is_absolute() or not re.fullmatch(r"[A-Za-z0-9_.-]+", name) or name in (".", ".."):
            raise PlatformError("wayland-session: supply absolute runtime directory and socket basename")
        endpoint = runtime / name
        _endpoint(endpoint, uid, socket=True)
        result += _mount(endpoint, "/run/atrinik-wayland")
        result += ["--env", "WAYLAND_DISPLAY=/run/atrinik-wayland", "--env", "SDL_VIDEODRIVER=wayland"]
    else:
        raise PlatformError("desktop-display: choose x11 or wayland")
    if gpu == "mesa":
        if not render_devices:
            raise PlatformError("mesa-render-device: select an accessible /dev/dri/renderD device")
        for device in render_devices:
            if device.parent != Path("/dev/dri") or not re.fullmatch(r"renderD[0-9]+", device.name):
                raise PlatformError("mesa-render-device: unsupported path")
            try:
                info = device.lstat()
            except OSError:
                raise PlatformError("mesa-render-device: missing device") from None
            if not stat.S_ISCHR(info.st_mode) or not os.access(device, os.R_OK | os.W_OK):
                raise PlatformError("mesa-render-device: inaccessible device")
            result += ["--device", str(device), "--group-add", str(info.st_gid)]
    elif gpu == "nvidia":
        result += ["--gpus", "all", "--env", "NVIDIA_DRIVER_CAPABILITIES=compute,utility,graphics,display"]
    else:
        raise PlatformError("desktop-gpu: choose mesa or nvidia; software is test-only")
    if audio:
        runtime = Path(environment.get("XDG_RUNTIME_DIR", ""))
        if not runtime.is_absolute():
            raise PlatformError("audio-session: absolute XDG_RUNTIME_DIR required")
        endpoint = runtime / "pulse/native"
        _endpoint(endpoint, uid, socket=True)
        result += _mount(endpoint, "/run/atrinik-pulse")
        result += ["--env", "PULSE_SERVER=unix:/run/atrinik-pulse", "--env", "SDL_AUDIODRIVER=pulse"]
    return result


def prerequisite_report(*, docker: bool = False) -> dict[str, object]:
    """Check tools separately from optional Docker and graphics capabilities."""
    missing = [name for name in ("python3", "git", "git-lfs", "cmake", "ninja", "pkg-config")
               if shutil.which(name) is None]
    failures = ["missing-tool:" + name for name in missing]
    if "git" not in missing and "git-lfs" not in missing:
        try:
            result = subprocess.run(["git", "lfs", "version"], capture_output=True, timeout=10, check=False)
            if result.returncode:
                failures.append("git-lfs-unusable")
            else:
                for key, expected in (("filter.lfs.process", "git-lfs filter-process"),
                                      ("filter.lfs.required", "true")):
                    configured = subprocess.run(["git", "config", "--get", key],
                                                capture_output=True, timeout=10, check=False, text=True)
                    if configured.returncode or configured.stdout.strip() != expected:
                        failures.append("git-lfs-filter:" + key)
        except (OSError, subprocess.TimeoutExpired):
            failures.append("git-lfs-unusable")
    if docker:
        if shutil.which("docker") is None:
            failures.append("missing-tool:docker")
        else:
            try:
                result = subprocess.run(["docker", "info", "--format", "{{.ServerVersion}}"],
                                        capture_output=True, timeout=15, check=False)
                if result.returncode:
                    failures.append("docker-daemon-access:check-daemon-and-user-permissions")
            except (OSError, subprocess.TimeoutExpired):
                failures.append("docker-daemon-unavailable")
    return {"schema_version": 1, "ok": not failures, "failures": failures,
            "graphics_required": False, "delivery_authority": "requires-coordinator-probe"}


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--docker", action="store_true", help="also check Docker daemon access")
    result = prerequisite_report(docker=parser.parse_args(argv).docker)
    print(json.dumps(result, sort_keys=True))
    return 0 if result["ok"] else 2


if __name__ == "__main__":
    raise SystemExit(main())
