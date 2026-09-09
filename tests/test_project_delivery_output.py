"""CLI snapshot handoff, compact results and publication failure behavior."""
from contextlib import redirect_stderr, redirect_stdout
import io
import json
import os
from pathlib import Path
import stat
import tempfile
import unittest
from unittest.mock import patch

from atrinik_workspace import project_delivery as cli
from atrinik_workspace.project_coordinator import digest
from atrinik_workspace.project_coordinator_store import Store, read_input
from tests.test_project_coordinator import FakeGitHub, plan, project


@unittest.skipUnless(os.name == "posix", "canonical Linux filesystem contract")
class SnapshotOutputTests(unittest.TestCase):
    def setUp(self):
        temporary = tempfile.TemporaryDirectory()
        self.addCleanup(temporary.cleanup)
        self.wrapper = Path(temporary.name)
        document = project()
        self.root = self.wrapper / "build" / "project-delivery" / digest({"parent": document["plan"]["parent"]})
        self.root.mkdir(parents=True, mode=0o700)
        self.initial = Store(self.root).create(document)
        context = patch.object(cli, "context", return_value=self.wrapper)
        context.start()
        self.addCleanup(context.stop)
        github = patch.object(cli, "GitHub", return_value=FakeGitHub())
        github.start()
        self.addCleanup(github.stop)

    def invoke(self, command, output=None, compact=False):
        argv = ["--root", str(self.root)]
        if output is not None:
            argv += ["--snapshot-output", str(output)]
        if compact:
            argv += ["--compact"]
        stdout, stderr = io.StringIO(), io.StringIO()
        with redirect_stdout(stdout), redirect_stderr(stderr):
            code = cli.main(argv + command)
        return code, json.loads(stdout.getvalue()) if stdout.getvalue() else None, stderr.getvalue()

    def test_exact_snapshot_supports_next_mutation_and_rejects_stale_cas(self):
        first, second = self.wrapper / "first.json", self.wrapper / "second.json"
        code, response, error = self.invoke(["inspect"], first, True)
        self.assertEqual((code, error), (0, ""))
        self.assertEqual(read_input(first), self.initial)
        self.assertNotIn("document", response["snapshot"])
        self.assertEqual(stat.S_IMODE(first.stat().st_mode), 0o600)
        command = ["dispatch", "--capacity", "2", "--open-workers", "0", "--expected", str(first)]
        code, dispatched, error = self.invoke(command, second, True)
        self.assertEqual((code, error), (0, ""))
        self.assertEqual(len(dispatched["result"]), 2)
        self.assertTrue(all(item["attempt"] for item in dispatched["result"]))
        self.assertEqual(read_input(second), Store(self.root).inspect())
        third = self.wrapper / "third.json"
        code, _, error = self.invoke(command, third, True)
        self.assertEqual(code, 2)
        self.assertIn("stale project CAS", error)
        self.assertFalse(third.exists())
        self.assertEqual(Store(self.root).inspect(), read_input(second))

    def test_default_output_and_full_export_keep_original_shape(self):
        for output in (None, self.wrapper / "full.json"):
            code, response, error = self.invoke(["inspect"], output)
            self.assertEqual((code, error), (0, ""))
            self.assertEqual(response, self.initial)

    def test_unsafe_destinations_fail_before_execution(self):
        regular = self.wrapper / "existing.json"
        regular.write_text("preserve")
        symlink = self.wrapper / "link.json"
        symlink.symlink_to(regular)
        hardlink = self.wrapper / "hardlink.json"
        os.link(regular, hardlink)
        public = self.wrapper / "public"
        public.mkdir(mode=0o755)
        for output in (regular, symlink, hardlink, self.root / "project.json", public / "out.json", Path("relative.json")):
            with self.subTest(output=output), patch.object(cli, "run") as run:
                code, _, _ = self.invoke(["inspect"], output)
                self.assertEqual(code, 2)
                run.assert_not_called()
        self.assertEqual(regular.read_text(), "preserve")
        self.assertEqual(Store(self.root).inspect(), self.initial)

    def test_compact_requires_export_and_non_snapshot_commands_fail_before_execution(self):
        for command, output in ((["inspect"], None), (["terminal"], self.wrapper / "terminal.json"),
                                (["plan", "--capacity", "1", "--open-workers", "0"], self.wrapper / "plan.json")):
            with patch.object(cli, "run") as run:
                self.assertEqual(self.invoke(command, output, True)[0], 2)
                run.assert_not_called()

    def test_publication_race_preserves_existing_file_and_explains_committed_mutation(self):
        expected, output = self.wrapper / "expected.json", self.wrapper / "output.json"
        self.invoke(["inspect"], expected)
        original = cli.run
        def raced(args):
            value = original(args)
            output.write_text("another caller")
            return value
        with patch.object(cli, "run", side_effect=raced):
            code, response, error = self.invoke(["dispatch", "--capacity", "1", "--open-workers", "0",
                                                 "--expected", str(expected)], output, True)
        self.assertEqual(code, 2)
        self.assertIsNone(response)
        self.assertIn("command completed", error)
        self.assertIn("reconcile before any retry", error)
        self.assertEqual(output.read_text(), "another caller")
        self.assertEqual(Store(self.root).inspect()["generation"], self.initial["generation"] + 1)
        self.assertEqual(list(self.wrapper.glob(".output.json.*.tmp")), [])

    def test_compact_init_retains_root_and_exports_full_authority(self):
        supplied, output = self.wrapper / "plan.json", self.wrapper / "init.json"
        supplied.write_text(json.dumps(plan()))
        stdout, stderr = io.StringIO(), io.StringIO()
        # Initialize a different canonical wrapper, preserving the setUp project.
        wrapper = self.wrapper / "new-wrapper"
        wrapper.mkdir(mode=0o700)
        with patch.object(cli, "context", return_value=wrapper), patch("subprocess.run") as run:
            run.return_value.returncode = 0
            with redirect_stdout(stdout), redirect_stderr(stderr):
                code = cli.main(["--snapshot-output", str(output), "--compact", "init", "--plan",
                                 str(supplied), "--authority", "test:complete-session-authority"])
        self.assertEqual((code, stderr.getvalue()), (0, ""))
        response = json.loads(stdout.getvalue())
        full = read_input(output)
        self.assertEqual(full, Store(Path(response["root"])).inspect())
        self.assertEqual(full["document"]["authority"], "test:complete-session-authority")
        self.assertLess(len(stdout.getvalue()), len(output.read_text()))

    def test_export_does_not_bypass_actor_gate(self):
        expected, output = self.wrapper / "expected.json", self.wrapper / "denied.json"
        self.invoke(["inspect"], expected)
        with patch.object(FakeGitHub, "actor", return_value="foreign-actor"):
            code, _, error = self.invoke(["dispatch", "--capacity", "1", "--open-workers", "0",
                                          "--expected", str(expected)], output, True)
        self.assertEqual(code, 2)
        self.assertIn("authenticated actor changed", error)
        self.assertFalse(output.exists())
        self.assertEqual(Store(self.root).inspect(), self.initial)

    def test_tracking_compact_bounds_stdout_and_preserves_exact_full_snapshot(self):
        for action in ("plan", "apply", "reconcile", "cancel"):
            with self.subTest(action=action):
                metadata = {"id": "operation-id", "kind": "comment", "target": "atrinik/atrinik#1",
                            "phase": "planned" if action == "plan" else "bound"}
                operation = dict(metadata, payload={"body": "p" * 16000},
                                 request={"body": "r" * 16000}, before={"body": "b" * 16000},
                                 result={"identity": "live-id", "body": "s" * 16000})
                snapshot = dict(self.initial, document={"operations": {"operation-id": operation}})
                value = {"snapshot": snapshot, "result": operation} if action == "plan" else snapshot
                output = self.wrapper / f"tracking-{action}.json"
                command = ["tracking", action, "--expected", str(self.wrapper / "unused")]
                command += (["--kind", "comment", "--target", "atrinik/atrinik#1", "--payload",
                             str(self.wrapper / "payload.json")] if action == "plan"
                            else ["--operation", "operation-id"])
                with patch.object(cli, "run", return_value=value):
                    code, response, error = self.invoke(command, output, True)
                self.assertEqual((code, error), (0, ""))
                self.assertEqual(response["result"], metadata)
                self.assertLess(len(json.dumps(response)), 1000)
                self.assertEqual(read_input(output), snapshot)


if __name__ == "__main__":
    unittest.main()
