from __future__ import annotations

import os
from pathlib import Path
import subprocess
import tempfile
import unittest


ROOT = Path(__file__).resolve().parents[1]
SCRIPT = ROOT / "scripts" / "server-worktree.sh"


class ServerWorktreeTests(unittest.TestCase):
    def make_server(self, root: Path) -> Path:
        server = root / "server"
        (server / "install_data" / "keys").mkdir(parents=True)
        (server / "install_data" / "unique-items").mkdir()
        (server / "install_data" / "bans").write_text("")
        (server / "install_data" / "motd").write_text("Welcome\n")
        (server / "install_data" / "seed.txt").write_text("seed\n")
        (server / "tools").mkdir()
        (server / "tools" / "prepare-runtime.sh").write_text(
            "#!/usr/bin/env bash\n"
            "set -eu\n"
            "root=$(cd \"$(dirname \"${BASH_SOURCE[0]}\")/..\" && pwd)\n"
            "printf '%s\\n' \"$1\" > \"${root}/prepared-build-dir\"\n"
        )
        (server / "tools" / "prepare-runtime.sh").chmod(0o755)
        (server / "server.sh").write_text(
            "#!/usr/bin/env bash\nset -eu\nprintf '%s\\n' \"$@\"\n"
        )
        (server / "server.sh").chmod(0o755)
        return server

    def run_helper(
        self, action: str, server: Path, state: Path, *arguments: str
    ) -> subprocess.CompletedProcess[str]:
        environment = os.environ.copy()
        environment["ATRINIK_SHARED_SERVER_DATA"] = str(state)
        return subprocess.run(
            [
                str(SCRIPT),
                action,
                "--worktree",
                str(server),
                "--build-dir",
                "build/test",
                *arguments,
            ],
            check=False,
            capture_output=True,
            text=True,
            env=environment,
            timeout=10,
        )

    def test_prepare_initializes_and_links_shared_data(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            server = self.make_server(root)
            state = root / "state" / "server-data"

            result = self.run_helper("prepare", server, state)

            self.assertEqual(result.returncode, 0, result.stderr)
            self.assertTrue((state / "seed.txt").is_file())
            self.assertTrue((state / "tmp").is_dir())
            self.assertTrue((server / "data").is_symlink())
            self.assertEqual((server / "data").resolve(), state.resolve())
            self.assertEqual(
                (server / "prepared-build-dir").read_text(), "build/test\n"
            )

    def test_prepare_refuses_existing_worktree_data(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            server = self.make_server(root)
            (server / "data").mkdir()
            state = root / "state" / "server-data"

            result = self.run_helper("prepare", server, state)

            self.assertNotEqual(result.returncode, 0)
            self.assertIn("refusing to replace existing", result.stderr)
            self.assertFalse(state.exists())

    def test_prepare_refuses_state_inside_worktree(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            server = self.make_server(root)
            state = server / "shared" / "server-data"

            result = self.run_helper("prepare", server, state)

            self.assertNotEqual(result.returncode, 0)
            self.assertIn("must be outside", result.stderr)
            self.assertFalse(state.exists())

    def test_prepare_refuses_another_data_symlink(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            server = self.make_server(root)
            other_state = root / "other-data"
            other_state.mkdir()
            (server / "data").symlink_to(other_state, target_is_directory=True)
            state = root / "state" / "server-data"

            result = self.run_helper("prepare", server, state)

            self.assertNotEqual(result.returncode, 0)
            self.assertIn("points elsewhere", result.stderr)

    def test_run_forwards_arguments(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            server = self.make_server(root)
            state = root / "state" / "server-data"

            result = self.run_helper(
                "run", server, state, "--", "--port_mapping=off", "--version"
            )

            self.assertEqual(result.returncode, 0, result.stderr)
            self.assertIn("--port_mapping=off\n--version\n", result.stdout)


if __name__ == "__main__":
    unittest.main()
