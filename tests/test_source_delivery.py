"""Ordinary source preparation using real disposable Git repositories."""
from contextlib import redirect_stdout
import io
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest
from unittest.mock import patch

from scripts import source_delivery as cli


@unittest.skipUnless(sys.platform.startswith("linux"), "native Linux source preparation")
class SourceDeliveryTests(unittest.TestCase):
    def setUp(self):
        temporary = tempfile.TemporaryDirectory()
        self.addCleanup(temporary.cleanup)
        self.root = Path(temporary.name)
        self.repository = self.root / "repository"
        self.repository.mkdir()
        self.git("init", "-b", "main")
        self.git("config", "user.name", "Source Test")
        self.git("config", "user.email", "source@example.invalid")
        (self.repository / "source.txt").write_text("original\n")
        self.git("add", "source.txt")
        self.git("commit", "-m", "initial")
        self.base = self.git("rev-parse", "HEAD")
        self.target = self.root / "source-work"

    def git(self, *args, root=None):
        return subprocess.check_output(["git", "-C", str(root or self.repository), *args],
                                       text=True, stderr=subprocess.PIPE).strip()

    def invoke(self, command="start", *, owner="session-one", branch="work/source", base=None,
               target=None, dirty=False):
        argv = [command, "--worktree", str(target or self.target), "--branch", branch,
                "--base=" + (base or self.base), "--owner", owner]
        if command == "start":
            argv += ["--repository", str(self.repository)]
        if dirty:
            argv.append("--allow-dirty")
        output = io.StringIO()
        with redirect_stdout(output):
            code = cli.main(argv)
        return code, json.loads(output.getvalue())

    def test_start_ignores_unrelated_journals_and_dirty_primary(self):
        journal = self.repository / "workspace" / "review" / "unrelated.ledger.json"
        journal.parent.mkdir(parents=True)
        journal.write_text("unrelated incomplete diagnostic")
        (self.repository / "source.txt").write_text("primary edits\n")
        code, result = self.invoke()
        self.assertEqual(code, 0, result)
        self.assertEqual(result["head"], self.base)
        self.assertEqual(result["base"], self.base)
        self.assertEqual(result["branch"], "work/source")
        self.assertEqual(result["worktree"], str(self.target))
        self.assertEqual(result["repository"], str(self.repository / ".git"))
        self.assertEqual(journal.read_text(), "unrelated incomplete diagnostic")
        self.assertEqual((self.repository / "source.txt").read_text(), "primary edits\n")
        self.assertEqual((self.target / "source.txt").read_text(), "original\n")

    def test_dirty_resume_needs_matching_owner_and_explicit_flag(self):
        self.assertEqual(self.invoke()[0], 0)
        edited = self.target / "source.txt"
        edited.write_text("owned edits\n")
        self.assertEqual(self.invoke("resume")[0], 2)
        self.assertEqual(self.invoke("resume", owner="another-session", dirty=True)[0], 2)
        code, result = self.invoke("resume", dirty=True)
        self.assertEqual(code, 0, result)
        self.assertTrue(result["dirty"])
        self.assertEqual(edited.read_text(), "owned edits\n")

    def test_clean_resume_accepts_committed_descendant(self):
        self.assertEqual(self.invoke()[0], 0)
        self.git("commit", "--allow-empty", "-m", "source change", root=self.target)
        code, result = self.invoke("resume")
        self.assertEqual(code, 0, result)
        self.assertNotEqual(result["head"], self.base)
        self.assertFalse(result["dirty"])

    def test_primary_and_unknown_worktree_refused(self):
        code, result = self.invoke("resume", target=self.repository, branch="main")
        self.assertEqual(code, 2)
        self.assertIn("Primary", result["error"])
        self.git("worktree", "add", "-b", "work/source", str(self.target), self.base)
        code, result = self.invoke("resume", dirty=True)
        self.assertEqual(code, 2)
        self.assertIn("No source creation receipt", result["error"])

    def test_collisions_and_managed_paths_preserved(self):
        self.target.mkdir()
        sentinel = self.target / "preserve"
        sentinel.write_text("keep")
        self.assertEqual(self.invoke()[0], 2)
        self.assertEqual(sentinel.read_text(), "keep")
        self.git("branch", "work/source")
        other = self.root / "other"
        self.assertEqual(self.invoke(target=other)[0], 2)
        self.assertFalse(other.exists())
        managed = self.repository / "workspace"
        managed.mkdir()
        self.assertEqual(self.invoke(target=managed / "new", branch="work/new")[0], 2)
        self.assertFalse((managed / "new").exists())

    def test_symlink_parent_and_receipt_refused(self):
        alias = self.root / "alias"
        alias.symlink_to(self.root, target_is_directory=True)
        self.assertEqual(self.invoke(target=alias / "new")[0], 2)
        self.assertEqual(self.invoke()[0], 0)
        gitdir = Path(self.git("rev-parse", "--absolute-git-dir", root=self.target))
        receipt = gitdir / cli.RECEIPT
        backup = gitdir / "receipt-backup"
        receipt.rename(backup)
        receipt.symlink_to(backup)
        self.assertEqual(self.invoke("resume")[0], 2)

    def test_branch_base_and_git_environment_cannot_redirect_work(self):
        self.assertEqual(self.invoke(branch="../invalid")[0], 2)
        self.assertEqual(self.invoke(base="--help")[0], 2)
        with patch.dict(os.environ, {"GIT_DIR": str(self.root / "missing"), "GIT_WORK_TREE": "/"}):
            self.assertEqual(self.invoke()[0], 0)
        self.assertEqual(self.invoke("resume", branch="work/wrong")[0], 2)

    def test_git_failure_keeps_existing_branch_and_reserved_path(self):
        original = cli.git

        def racing_git(root, *args):
            if args[:2] == ("worktree", "add"):
                self.git("branch", "work/source")
            return original(root, *args)

        with patch.object(cli, "git", side_effect=racing_git):
            code, result = self.invoke()
        self.assertEqual(code, 2)
        self.assertIn("Resources were preserved", result["error"])
        self.assertTrue(self.target.is_dir())
        self.assertEqual(self.git("rev-parse", "work/source"), self.base)

    def test_receipt_failure_preserves_created_source(self):
        original = cli.os.open

        def failing_open(path, *args, **kwargs):
            if str(path).endswith(cli.RECEIPT):
                raise OSError("receipt write unavailable")
            return original(path, *args, **kwargs)

        with patch.object(cli.os, "open", side_effect=failing_open):
            code, result = self.invoke()
        self.assertEqual(code, 2)
        self.assertIn("Resources were preserved", result["error"])
        self.assertEqual((self.target / "source.txt").read_text(), "original\n")
        self.assertEqual(self.git("rev-parse", "work/source"), self.base)
        self.assertEqual(self.invoke("resume")[0], 2)

    def test_base_drift_and_locked_worktree_refused(self):
        self.assertEqual(self.invoke()[0], 0)
        self.git("commit", "--allow-empty", "-m", "primary advance")
        self.assertEqual(self.invoke("resume", base="main")[0], 2)
        self.git("worktree", "lock", str(self.target))
        self.assertEqual(self.invoke("resume")[0], 2)

    def test_ignored_files_still_need_explicit_dirty_resume(self):
        self.assertEqual(self.invoke()[0], 0)
        exclude = self.repository / ".git" / "info" / "exclude"
        with exclude.open("a") as stream:
            stream.write("\nignored-output\n")
        (self.target / "ignored-output").write_text("owned output")
        self.assertEqual(self.invoke("resume")[0], 2)
        code, result = self.invoke("resume", dirty=True)
        self.assertEqual(code, 0, result)
        self.assertTrue(result["dirty"])


if __name__ == "__main__":
    unittest.main()
