"""CLI snapshot handoff, compact results and publication failure behavior."""
from contextlib import redirect_stderr, redirect_stdout
import io
import json
import multiprocessing
import os
from pathlib import Path
import stat
import tempfile
import unittest
from unittest.mock import patch

from atrinik_workspace import project_delivery as cli
from atrinik_workspace.project_coordinator import ProjectError, digest, reserve_existing
from atrinik_workspace.project_coordinator_store import Store, read_input
from tests.test_project_coordinator import FakeGitHub, plan, project, retired_project, runtime_observation


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


def race_existing_reservation(root, expected, observation, start, result):
    start.wait(10)
    try:
        snapshot, request = Store(Path(root)).update(expected, lambda p: reserve_existing(
            p, "atrinik/atrinik#2", "/root/leaf", read_input(Path(observation)), expected, 1))
        result.put(("reserved", request["attempt"], snapshot["generation"]))
    except (ProjectError, OSError) as error:
        result.put(("refused", str(error)))


@unittest.skipUnless(os.name == "posix", "canonical Linux filesystem contract")
class ExistingReservationOutputTests(unittest.TestCase):
    invoke = SnapshotOutputTests.invoke
    # Reuse the CLI fixture and invoke helper without inheriting unrelated tests.
    def setUp(self):
        temporary = tempfile.TemporaryDirectory()
        self.addCleanup(temporary.cleanup)
        self.wrapper = Path(temporary.name)
        document = retired_project()
        self.root = self.wrapper / "build" / "project-delivery" / digest({"parent": document["plan"]["parent"]})
        self.root.mkdir(parents=True, mode=0o700)
        self.initial = Store(self.root).create(document)
        for mocked in (patch.object(cli, "context", return_value=self.wrapper),
                       patch.object(cli, "GitHub", return_value=FakeGitHub())):
            mocked.start(); self.addCleanup(mocked.stop)
        self.expected = self.wrapper / "expected.json"
        self.observation = self.wrapper / "runtime.json"
        self.invoke(["inspect"], self.expected)
        self.observation.write_text(json.dumps(runtime_observation(document, self.initial)))
        self.observation.chmod(0o600)
        self.command = ["reserve-existing", "atrinik/atrinik#2", "--worker", "/root/leaf",
                        "--runtime-observation", str(self.observation), "--expected", str(self.expected)]

    def test_reuse_compact_output_is_durable_and_old_snapshot_cannot_reserve_again(self):
        output = self.wrapper / "reserved.json"
        code, result, error = self.invoke(self.command, output, True)
        self.assertEqual((code, error), (0, ""))
        self.assertEqual(result["result"]["worker"], "/root/leaf")
        snapshot = read_input(output)
        self.assertEqual(snapshot, Store(self.root).inspect())
        state = snapshot["document"]["nodes"]["atrinik/atrinik#2"]
        self.assertEqual((state["worker"], state["state"]), ("/root/leaf", "reserved"))
        self.assertEqual(state["attempt"], result["result"]["attempt"])
        calls = []
        original = cli.read_input
        def observed(path, *args):
            calls.append(path); return original(path, *args)
        with patch.object(cli, "read_input", side_effect=observed):
            code, _, error = self.invoke(self.command)
        self.assertEqual(code, 2); self.assertIn("stale project CAS", error)
        self.assertNotIn(self.observation, calls)
        self.assertEqual(Store(self.root).inspect(), snapshot)
        code, _, error = self.invoke(["worker", "atrinik/atrinik#2", "--attempt", state["attempt"],
                                     "--id", "/root/other", "--expected", str(output)])
        self.assertEqual(code, 2); self.assertIn("takeover", error)
        self.assertEqual(Store(self.root).inspect(), snapshot)

    def test_observation_read_occurs_inside_cas_change_callback(self):
        original_read, original_update = cli.read_input, Store.update
        inside = False
        def update(store, expected, change):
            def guarded(project):
                nonlocal inside
                inside = True
                try: return change(project)
                finally: inside = False
            return original_update(store, expected, guarded)
        def read(path, *args):
            if path == self.observation: self.assertTrue(inside)
            return original_read(path, *args)
        with patch.object(Store, "update", update), patch.object(cli, "read_input", side_effect=read):
            self.assertEqual(self.invoke(self.command)[0], 0)

    def test_unsafe_and_oversized_runtime_files_preserve_project(self):
        link = self.wrapper / "link.json"; link.symlink_to(self.observation)
        hard = self.wrapper / "hard.json"; os.link(self.observation, hard)
        fifo = self.wrapper / "pipe.json"; os.mkfifo(fifo, 0o600)
        huge = self.wrapper / "huge.json"; huge.write_text(" " * (128 * 1024 + 1)); huge.chmod(0o600)
        duplicate = self.wrapper / "duplicate.json"; duplicate.write_text('{"a":1,"a":2}'); duplicate.chmod(0o600)
        for path in (link, hard, fifo, huge, duplicate, self.wrapper / "missing.json"):
            with self.subTest(path=path):
                command = list(self.command); command[command.index("--runtime-observation") + 1] = str(path)
                self.assertEqual(self.invoke(command)[0], 2)
                self.assertEqual(Store(self.root).inspect(), self.initial)

    def test_lost_output_preserves_prebound_reservation_and_requires_reconciliation(self):
        with patch.object(cli, "publish_result", side_effect=OSError("lost output")):
            code, result, error = self.invoke(self.command, self.wrapper / "lost.json", True)
        self.assertEqual(code, 2); self.assertIsNone(result)
        self.assertIn("command completed", error)
        self.assertIn("reconcile before any retry", error)
        current = Store(self.root).inspect()
        state = current["document"]["nodes"]["atrinik/atrinik#2"]
        self.assertEqual((state["worker"], state["state"]), ("/root/leaf", "reserved"))
        self.assertEqual(self.invoke(self.command)[0], 2)
        self.assertEqual(Store(self.root).inspect(), current)

    def test_concurrent_same_cas_reservations_have_one_winner(self):
        ctx = multiprocessing.get_context("fork")
        start, result = ctx.Event(), ctx.Queue()
        workers = [ctx.Process(target=race_existing_reservation,
                    args=(str(self.root), self.initial, str(self.observation), start, result)) for _ in range(2)]
        for worker in workers: worker.start()
        start.set()
        try:
            outcomes = [result.get(timeout=10) for _ in workers]
            for worker in workers:
                worker.join(10); self.assertEqual(worker.exitcode, 0)
            self.assertEqual(sorted(o[0] for o in outcomes), ["refused", "reserved"])
            current = Store(self.root).inspect()
            self.assertEqual(current["generation"], self.initial["generation"] + 1)
            self.assertEqual(current["document"]["nodes"]["atrinik/atrinik#2"]["attempt"],
                             next(o[1] for o in outcomes if o[0] == "reserved"))
        finally:
            for worker in workers:
                if worker.is_alive(): worker.terminate(); worker.join(10)
            result.close(); result.join_thread()


if __name__ == "__main__":
    unittest.main()
