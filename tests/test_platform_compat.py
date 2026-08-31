from __future__ import annotations

from contextlib import redirect_stderr
from io import StringIO
from pathlib import Path
import tempfile
import unittest
from unittest import mock

from atrinik_workspace import cli
from atrinik_workspace.locking import (
    LeaseRequest,
    active_lock_fds,
    resource_lifetime_reader,
)
from atrinik_workspace.model import atomic_json, durable_atomic_json, load_json
from atrinik_workspace.platform_compat import IS_WINDOWS, fcntl
from atrinik_workspace import platform_compat


class PlatformPathSafetyTests(unittest.TestCase):
    def test_real_and_missing_components_are_accepted(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            nested = root / "nested"
            nested.mkdir()
            file_path = nested / "record.json"
            file_path.write_text("{}", encoding="utf-8")

            platform_compat.assert_no_symlink_components(file_path, "test")
            platform_compat.assert_no_symlink_components(
                root / "missing" / "record.json", "test"
            )

    def test_symlink_components_are_rejected(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            with mock.patch.object(platform_compat.os.path, "islink", return_value=True):
                with self.assertRaisesRegex(OSError, "refusing symlinked test path"):
                    platform_compat.assert_no_symlink_components(
                        Path(temporary) / "record.json", "test"
                    )

    def test_non_directory_parent_is_rejected(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            (root / "record").write_text("{}", encoding="utf-8")
            with self.assertRaisesRegex(OSError, "test parent is not a directory"):
                platform_compat.assert_no_symlink_components(
                    root / "record" / "child", "test"
                )

    def test_component_inspection_errors_are_contextualized(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            with mock.patch.object(Path, "lstat", side_effect=OSError("blocked")):
                with self.assertRaisesRegex(OSError, "cannot inspect test path"):
                    platform_compat.assert_no_symlink_components(
                        Path(temporary) / "record.json", "test"
                    )


@unittest.skipUnless(IS_WINDOWS, "native Windows compatibility coverage")
class NativeWindowsLockTests(unittest.TestCase):
    def test_kernel_lock_is_exclusive_across_handles(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            path = Path(temporary) / "lease.lock"
            with path.open("a+") as first, path.open("a+") as second:
                fcntl.flock(first, fcntl.LOCK_EX | fcntl.LOCK_NB)
                with self.assertRaises(BlockingIOError):
                    fcntl.flock(second, fcntl.LOCK_EX | fcntl.LOCK_NB)
                fcntl.flock(first, fcntl.LOCK_UN)
                fcntl.flock(second, fcntl.LOCK_EX | fcntl.LOCK_NB)

    def test_atomic_json_round_trip_uses_native_path(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            path = Path(temporary) / "nested" / "record.json"
            atomic_json(path, {"value": 1})
            durable_atomic_json(path, {"value": 2})
            self.assertEqual(load_json(path), {"value": 2})

    def test_lifetime_lock_handle_is_inherited_by_child_processes(self) -> None:
        request = LeaseRequest(
            "source",
            "native-windows-wrapper",
            "shared",
            "test wrapper inheritance",
            "retry after the test operation finishes",
        )
        with tempfile.TemporaryDirectory() as temporary:
            with resource_lifetime_reader(Path(temporary), request) as lease:
                self.assertIn(lease.fileno(), active_lock_fds())

    def test_linux_only_cli_command_is_stable_and_actionable(self) -> None:
        error = StringIO()
        with redirect_stderr(error):
            result = cli.main(["up"])
        self.assertEqual(result, 1)
        self.assertIn("unavailable on native Windows", error.getvalue())
        self.assertIn("Linux", error.getvalue())
