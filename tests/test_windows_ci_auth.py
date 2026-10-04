# Copyright 2026 The Atrinik Project
# SPDX-License-Identifier: MIT
"""Execute the Windows initializer's ephemeral credential helper without a secret."""
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import textwrap
import unittest
from unittest.mock import patch


ROOT = Path(__file__).resolve().parents[1]
TOKEN = "synthetic-ci-token"


class WindowsCIAuthTests(unittest.TestCase):
    def setUp(self):
        workflow = (ROOT / ".github/workflows/integration.yml").read_text()
        block = workflow.split("      - name: Initialize the complete Classic cohort\n", 1)[1]
        block = block.split("      - name: Validate native Windows status", 1)[0]
        self.assertIn("        shell: python\n", block)
        self.assertIn("ATRINIK_CI_READ_TOKEN: ${{ github.token }}", block)
        self.program = compile(textwrap.dedent(block.split("        run: |\n", 1)[1]), "windows-init", "exec")
        self.temp = tempfile.TemporaryDirectory(prefix="ci auth fixture ")
        self.addCleanup(self.temp.cleanup)
        self.helper = Path(self.temp.name) / "credential.py"
        self.environment = None
        self.created_helper = None

    def execute_step(self, *, status=0, error=None):
        def initialize(command, *, env):
            self.assertEqual(command, [sys.executable, "atrinik", "init", "--with", "classic", "--jobs", "4"])
            self.assertNotIn(TOKEN, " ".join(command))
            self.created_helper = Path(env["ATRINIK_CI_GIT_HELPER"])
            self.helper.write_bytes(self.created_helper.read_bytes())
            self.environment = env.copy()
            self.environment["ATRINIK_CI_GIT_HELPER"] = self.helper.as_posix()
            if error:
                raise error
            return subprocess.CompletedProcess(command, status)
        with patch.dict(os.environ, {"ATRINIK_CI_READ_TOKEN": TOKEN}), \
                patch("subprocess.run", side_effect=initialize):
            before = dict(os.environ)
            with self.assertRaises(type(error) if error else SystemExit) as raised:
                exec(self.program, {})
            changed_keys = {key for key in set(os.environ) | set(before)
                            if os.environ.get(key) != before.get(key)}
            self.assertEqual(changed_keys, set())
        self.assertFalse(self.created_helper.exists())
        self.assertFalse(self.created_helper.parent.exists())
        if not error:
            self.assertEqual(raised.exception.code, status)
        self.assertNotIn(TOKEN, self.helper.read_text())
        self.assertNotIn(TOKEN, " ".join(value for key, value in self.environment.items() if key.startswith("GIT_CONFIG_")))

    def helper_call(self, request, *, operation="get", token=TOKEN):
        return subprocess.run([sys.executable, str(self.helper), operation], input=request,
                              text=True, capture_output=True, check=True,
                              env=dict(self.environment, ATRINIK_CI_READ_TOKEN=token))

    def test_cleanup_preserves_success_failure_and_exception(self):
        for status in (0, 1, 23):
            with self.subTest(status=status):
                self.execute_step(status=status)
        self.execute_step(error=OSError("synthetic subprocess failure"))

    def test_helper_exact_scope_and_nonpersistent_operations(self):
        self.execute_step()
        for path in ("atrinik/content.git", "atrinik/sound.git", "atrinik/content.git/info/lfs", "atrinik/sound.git/info/lfs"):
            result = self.helper_call(f"protocol=https\nhost=github.com\npath={path}\n\n")
            self.assertEqual(result.stdout, f"username=x-access-token\npassword={TOKEN}\n\n")
            self.assertEqual(result.stderr, "")
        valid = "protocol=https\nhost=github.com\npath=atrinik/content.git\n"
        metadata = ("capability[]=authtype\ncapability[]=state\n"
                    "wwwauth[]=Basic realm=fixture\nwwwauth[]=Basic realm=other\n"
                    "state[]=first\nstate[]=second\n")
        self.assertIn("password=" + TOKEN, self.helper_call(metadata + valid + "\n").stdout)
        for request in (
            valid.replace("https", "http"),
            valid.replace("github.com", "github.com.attacker.invalid"),
            valid.replace("github.com", "api.github.com"),
            valid.replace("github.com", "github.com:443"),
            valid.replace("atrinik/content.git", "other/content.git"),
            valid.replace("atrinik/content.git", "atrinik/content.git.evil"),
            valid.replace("atrinik/content.git", "atrinik/content.git/../other.git"),
            valid.replace("atrinik/content.git", "atrinik/content.git/info/lfs/objects/batch"),
            valid.replace("path=atrinik/content.git\n", ""),
            valid + "username=other\n",
            valid + "host=github.com\n",
            valid + "malformed\n",
        ):
            with self.subTest(request=request):
                self.assertEqual(self.helper_call(request + "\n").stdout, "")
        for operation in ("store", "erase", "unknown"):
            self.assertEqual(self.helper_call(valid + "\n", operation=operation).stdout, "")
        for token in ("", "bad\npassword=injected", "bad\rpassword=injected"):
            self.assertEqual(self.helper_call(valid + "\n", token=token).stdout, "")

    def test_git_credential_fill_honors_path_and_resets_inherited_helpers(self):
        self.execute_step()
        global_config = Path(self.temp.name) / "gitconfig"
        global_config.write_text('[credential]\n\thelper = "!printf \'username=wrong\\npassword=inherited\\n\'"\n')
        environment = dict(self.environment, GIT_CONFIG_GLOBAL=str(global_config),
                           GIT_CONFIG_NOSYSTEM="1", GIT_ASKPASS="", GIT_TERMINAL_PROMPT="0")
        for url, accepted in (
            ("https://github.com/atrinik/content.git", True),
            ("https://github.com/atrinik/sound.git/info/lfs", True),
            ("https://github.com/atrinik/private.git", False),
            ("https://github.com/atrinik/content.git.evil", False),
            ("https://github.com/other/content.git", False),
            ("https://evil.invalid/atrinik/content.git", False),
            ("http://github.com/atrinik/content.git", False),
        ):
            with self.subTest(url=url):
                result = subprocess.run(["git", "credential", "fill"], cwd=self.temp.name,
                                        input=("capability[]=authtype\ncapability[]=state\n"
                                               "wwwauth[]=Basic realm=fixture\nwwwauth[]=Basic realm=other\n"
                                               "url=" + url + "\n\n"), env=environment,
                                        text=True, capture_output=True)
                self.assertEqual(result.returncode == 0, accepted)
                self.assertNotIn("password=inherited", result.stdout)
                if accepted:
                    self.assertIn("password=" + TOKEN, result.stdout)
                else:
                    self.assertNotIn(TOKEN, result.stdout + result.stderr)
        self.assertEqual(sorted(path.name for path in Path(self.temp.name).iterdir()), ["credential.py", "gitconfig"])


if __name__ == "__main__":
    unittest.main()
