from __future__ import annotations

from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import unittest

from atrinik_workspace.locking import exclusive_lock
from atrinik_workspace.model import WorkspaceError


class NamedLockTests(unittest.TestCase):
    def test_same_path_lock_remains_exclusive_across_processes(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            path = Path(temporary) / "resource.lock"
            script = """import sys
from pathlib import Path
from atrinik_workspace.locking import exclusive_lock, LockBusyError
try:
    with exclusive_lock(Path(sys.argv[1]), 'test', nonblocking=True):
        pass
except LockBusyError:
    sys.exit(7)
"""
            with exclusive_lock(path, "test"):
                busy = subprocess.run([sys.executable, "-c", script, str(path)], capture_output=True, text=True, timeout=10)
                self.assertEqual(busy.returncode, 7, busy.stderr)
            released = subprocess.run([sys.executable, "-c", script, str(path)], capture_output=True, text=True, timeout=10)
            self.assertEqual(released.returncode, 0, released.stderr)

    def test_completed_lock_can_be_copied_to_new_storage(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            path = Path(temporary) / "resource.lock"
            with exclusive_lock(path, "test"):
                pass
            replacement = path.with_suffix(".copy")
            shutil.copyfile(path, replacement)
            replacement.replace(path)
            with exclusive_lock(path, "test", nonblocking=True):
                pass

    def test_lock_symlink_is_rejected(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            target = root / "target"
            target.write_text("preserve")
            path = root / "resource.lock"
            path.symlink_to(target)
            with self.assertRaises(WorkspaceError):
                with exclusive_lock(path, "test"):
                    self.fail("symlink admitted")
            self.assertEqual(target.read_text(), "preserve")
