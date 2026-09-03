from __future__ import annotations

import os
from pathlib import Path
import unittest
from unittest import mock

from atrinik_workspace.docker_storage import (
    VOLUME_NAMESPACE_ENV,
    volume_name,
    volume_namespace,
    windows_package_volume_mounts,
)
from atrinik_workspace.model import WorkspaceError


class DockerStorageTests(unittest.TestCase):
    def test_windows_package_mounts_are_namespaced_and_no_copy(self) -> None:
        with mock.patch.dict(
            os.environ,
            {VOLUME_NAMESPACE_ENV: "dev-abc123"},
            clear=False,
        ):
            mounts = windows_package_volume_mounts(Path("/tmp/checkout"))

        self.assertEqual(
            [mount.name for mount in mounts],
            [
                "atrinik-dev-abc123-windows-client-build",
                "atrinik-dev-abc123-windows-server-build",
                "atrinik-dev-abc123-windows-compiler-cache",
                "atrinik-dev-abc123-windows-dependency-downloads",
            ],
        )
        self.assertEqual(
            [mount.target for mount in mounts],
            [
                "/workspace/client/build",
                "/workspace/server/build",
                "/workspace/.ccache",
                "/workspace/.dependency-downloads",
            ],
        )
        self.assertTrue(
            all("volume-nocopy" in mount.docker_spec for mount in mounts)
        )

    def test_path_namespace_is_stable_without_environment_override(self) -> None:
        with mock.patch.dict(os.environ, {}, clear=True):
            namespace = volume_namespace(Path("/tmp/private-checkout"))
            mounts = windows_package_volume_mounts(Path("/tmp/private-checkout"))

        self.assertRegex(namespace, r"^path-[0-9a-f]{16}$")
        self.assertTrue(
            all("private-checkout" not in mount.name for mount in mounts)
        )

    def test_invalid_namespace_and_purpose_fail_closed(self) -> None:
        with mock.patch.dict(
            os.environ,
            {VOLUME_NAMESPACE_ENV: "unsafe namespace"},
            clear=False,
        ):
            with self.assertRaisesRegex(WorkspaceError, "lowercase Docker-safe"):
                volume_namespace(Path("/tmp/checkout"))

        with mock.patch.dict(os.environ, {}, clear=True):
            with self.assertRaisesRegex(
                WorkspaceError, "invalid Docker volume purpose"
            ):
                volume_name(Path("/tmp/checkout"), "../build")


if __name__ == "__main__":
    unittest.main()
