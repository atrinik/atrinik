from __future__ import annotations

from contextlib import redirect_stderr
from io import StringIO
from pathlib import Path
import tempfile
import unittest

from atrinik_workspace import cli
from atrinik_workspace.locking import (
    LeaseRequest,
    active_lock_fds,
    resource_lifetime_reader,
)
from atrinik_workspace.model import atomic_json, durable_atomic_json, load_json
from atrinik_workspace.platform_compat import IS_WINDOWS, fcntl


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
