from __future__ import annotations

from concurrent.futures import ThreadPoolExecutor
import os
from pathlib import Path
import subprocess
import tempfile
import unittest
from unittest import mock

from atrinik_workspace import agent_ledgers
from atrinik_workspace.agent_ledgers import (
    AgentLedgerCommitUncertain,
    AgentLedgerError,
    ledger_lock_path,
    ledger_path,
    resolve_shared_root,
    update_agent_ledger,
)
from atrinik_workspace.guidance_inventory import (
    validate_process_improvement_ledger,
    validate_tooling_ledger,
)
from atrinik_workspace.locking import exclusive_lock


class AgentLedgerTests(unittest.TestCase):
    def setUp(self) -> None:
        self.temporary = tempfile.TemporaryDirectory()
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name)
        subprocess.run(
            ["git", "init", "--quiet"],
            cwd=self.root,
            check=True,
            stdout=subprocess.DEVNULL,
            stderr=subprocess.DEVNULL,
        )
        (self.root / ".gitignore").write_text("/build/\n", encoding="utf-8")

    def _tooling(self, key: str, observation: str = "observed") -> dict[str, object]:
        return update_agent_ledger(
            self.root,
            "tooling-issues",
            key=key,
            status="open",
            observation=observation,
            impact="the operation needs a bounded retry",
            recommended_action="reread the latest helper output",
        )

    def _process(self, key: str, observation: str = "observed") -> dict[str, object]:
        return update_agent_ledger(
            self.root,
            "process-improvements",
            key=key,
            status="observed",
            observation=observation,
            expected_benefit="keep the shared workflow bounded",
            related="none",
            observed_at="2026-09-03T00:00:00Z",
        )

    def test_each_schema_can_be_created_and_validated(self) -> None:
        tooling = self._tooling(
            "mechanism=local-ledger;remediation=shared-lock"
        )
        process = self._process("local-ledger")

        self.assertEqual(tooling["operation"], "added")
        self.assertEqual(process["operation"], "added")
        self.assertEqual(validate_tooling_ledger(self.root), [])
        self.assertEqual(validate_process_improvement_ledger(self.root), [])
        self.assertNotEqual(tooling["digest"], process["digest"])
        self.assertTrue((self.root / "build/.agent-ledgers.lock").is_file())

    def test_concurrent_different_keys_preserve_every_row(self) -> None:
        keys = [
            f"mechanism=worker-{index};remediation=atomic-merge"
            for index in range(12)
        ]
        with ThreadPoolExecutor(max_workers=len(keys)) as workers:
            results = list(workers.map(self._tooling, keys))

        self.assertEqual(len(results), len(keys))
        self.assertTrue(all(result["operation"] == "added" for result in results))
        self.assertEqual(validate_tooling_ledger(self.root), [])
        text = ledger_path(self.root, "tooling-issues").read_text(encoding="utf-8")
        for key in keys:
            self.assertEqual(text.count(f"`{key}`"), 1)

    def test_concurrent_same_key_keeps_one_valid_latest_row(self) -> None:
        key = "mechanism=same-key;remediation=serialized"
        observations = [f"writer-{index}" for index in range(10)]
        with ThreadPoolExecutor(max_workers=len(observations)) as workers:
            results = list(
                workers.map(
                    lambda observation: self._tooling(key, observation), observations
                )
            )

        self.assertEqual(len(results), len(observations))
        self.assertEqual(validate_tooling_ledger(self.root), [])
        text = ledger_path(self.root, "tooling-issues").read_text(encoding="utf-8")
        self.assertEqual(text.count(f"`{key}`"), 1)
        self.assertTrue(any(observation in text for observation in observations))

    def test_stale_digest_fails_closed_for_both_schemas(self) -> None:
        for ledger in ("tooling-issues", "process-improvements"):
            with self.subTest(ledger=ledger):
                if ledger == "tooling-issues":
                    initial = self._tooling(
                        "mechanism=stale-cas;remediation=first"
                    )
                    self._tooling(
                        "mechanism=stale-cas;remediation=first",
                        "newer observation",
                    )
                else:
                    initial = self._process("stale-cas")
                    self._process("stale-cas", "newer observation")
                with self.assertRaisesRegex(AgentLedgerError, "stale digest"):
                    if ledger == "tooling-issues":
                        update_agent_ledger(
                            self.root,
                            ledger,
                            key="mechanism=stale-cas;remediation=first",
                            status="open",
                            observation="stale observation",
                            impact="the operation needs a bounded retry",
                            recommended_action="reread the latest helper output",
                            expected_digest=initial["digest"],
                        )
                    else:
                        update_agent_ledger(
                            self.root,
                            ledger,
                            key="stale-cas",
                            status="observed",
                            observation="stale observation",
                            expected_benefit="keep the shared workflow bounded",
                            related="none",
                            observed_at="2026-09-03T00:00:00Z",
                            expected_digest=initial["digest"],
                        )

    def test_absent_digest_is_an_explicit_compare_and_swap(self) -> None:
        result = self._tooling(
            "mechanism=absent-cas;remediation=explicit", observation="first"
        )
        self.assertEqual(result["previous_digest"], None)
        self.assertEqual(result["operation"], "added")

        with self.assertRaisesRegex(AgentLedgerError, "stale digest"):
            update_agent_ledger(
                self.root,
                "tooling-issues",
                key="mechanism=absent-cas;remediation=explicit",
                status="open",
                observation="first",
                impact="the operation needs a bounded retry",
                recommended_action="reread the latest helper output",
                expected_digest="absent",
            )

    def test_nonblocking_contention_has_retry_diagnostic(self) -> None:
        self._tooling("mechanism=lock-test;remediation=retry")
        with exclusive_lock(ledger_lock_path(self.root), "test ledger holder"):
            with self.assertRaisesRegex(AgentLedgerError, "lock is busy"):
                update_agent_ledger(
                    self.root,
                    "tooling-issues",
                    key="mechanism=lock-test;remediation=retry",
                    status="open",
                    observation="contended",
                    impact="the operation needs a bounded retry",
                    recommended_action="reread the latest helper output",
                    nonblocking=True,
                )

    def test_failed_flush_preserves_previous_bytes_and_retry_succeeds(self) -> None:
        self._tooling("mechanism=flush-failure;remediation=retry", "before")
        path = ledger_path(self.root, "tooling-issues")
        before = path.read_bytes()
        with mock.patch.object(
            agent_ledgers, "flush_file", side_effect=OSError("simulated crash")
        ):
            with self.assertRaisesRegex(AgentLedgerError, "publication failed"):
                self._tooling("mechanism=flush-failure;remediation=retry", "after")
        self.assertEqual(path.read_bytes(), before)
        self.assertEqual(list(path.parent.glob(f".{path.name}.*.tmp")), [])
        retry = self._tooling("mechanism=flush-failure;remediation=retry", "after")
        self.assertEqual(retry["operation"], "updated")
        self.assertEqual(validate_tooling_ledger(self.root), [])

    def test_post_replace_durability_failure_is_explicit_and_retryable(self) -> None:
        self._tooling("mechanism=directory-sync;remediation=retry", "before")
        real_fsync = os.fsync
        calls = 0

        def fail_directory_sync(descriptor: int) -> None:
            nonlocal calls
            calls += 1
            if calls == 2:
                raise OSError("simulated directory sync failure")
            real_fsync(descriptor)

        with mock.patch.object(os, "fsync", side_effect=fail_directory_sync):
            with self.assertRaises(AgentLedgerCommitUncertain):
                self._tooling("mechanism=directory-sync;remediation=retry", "after")
        self.assertIn("after", ledger_path(self.root, "tooling-issues").read_text())
        retry = self._tooling("mechanism=directory-sync;remediation=retry", "after")
        self.assertEqual(retry["operation"], "unchanged")

    def test_malformed_current_bytes_are_never_replaced(self) -> None:
        fixtures = {
            "tooling-issues": (
                b"# Agent tooling issues\n\n| Stable key | Status |\n",
                "mechanism=malformed;remediation=tooling",
            ),
            "process-improvements": (
                b"# Agent process improvements\n\n| Key | Status |\n",
                "malformed-process",
            ),
        }
        for ledger, (malformed, key) in fixtures.items():
            with self.subTest(ledger=ledger):
                path = ledger_path(self.root, ledger)
                path.parent.mkdir(parents=True, exist_ok=True)
                path.write_bytes(malformed)
                with self.assertRaisesRegex(AgentLedgerError, "malformed"):
                    if ledger == "tooling-issues":
                        self._tooling(key)
                    else:
                        self._process(key)
                self.assertEqual(path.read_bytes(), malformed)

    def test_input_and_local_only_guards_fail_closed(self) -> None:
        with self.assertRaisesRegex(AgentLedgerError, "invalid tooling-issue stable key"):
            self._tooling("not-a-tooling-key")
        with self.assertRaisesRegex(AgentLedgerError, "without pipes"):
            self._tooling("mechanism=pipe;remediation=reject", "bad | row")

        (self.root / ".gitignore").write_text("/other/\n", encoding="utf-8")
        with self.assertRaisesRegex(AgentLedgerError, "non-ignored"):
            self._tooling("mechanism=not-ignored;remediation=reject")

    def test_stable_lock_local_only_guard_fails_closed(self) -> None:
        self._tooling("mechanism=tracked-lock;remediation=reject")
        lock = ledger_lock_path(self.root)
        subprocess.run(
            ["git", "add", "--force", "--", lock.relative_to(self.root).as_posix()],
            cwd=self.root,
            check=True,
            stdout=subprocess.DEVNULL,
            stderr=subprocess.DEVNULL,
        )
        path = ledger_path(self.root, "tooling-issues")
        before = path.read_bytes()
        with self.assertRaisesRegex(AgentLedgerError, "tracked ledger path"):
            self._tooling("mechanism=tracked-lock;remediation=reject", "blocked")
        self.assertEqual(path.read_bytes(), before)

    @unittest.skipIf(os.name == "nt", "native Windows has no directory descriptor pin")
    def test_replaced_build_directory_is_rejected_before_read(self) -> None:
        self._tooling("mechanism=directory-replacement;remediation=reject")
        build = self.root / "build"
        original = self.root / "build-original"
        path = ledger_path(self.root, "tooling-issues")
        with agent_ledgers._opened_build_directory(build) as directory:
            build.rename(original)
            build.mkdir()
            with self.assertRaisesRegex(AgentLedgerError, "changed during publication"):
                agent_ledgers._read_snapshot(path, directory)
        self.assertTrue((original / path.name).is_file())
        self.assertFalse(path.exists())

    @unittest.skipIf(os.name == "nt", "native Windows has different symlink privileges")
    def test_symlink_target_is_rejected_without_following_it(self) -> None:
        outside = self.root / "outside.md"
        outside.write_text("do not touch\n", encoding="utf-8")
        path = ledger_path(self.root, "tooling-issues")
        path.parent.mkdir(parents=True, exist_ok=True)
        path.symlink_to(outside)
        with self.assertRaises(AgentLedgerError):
            self._tooling("mechanism=symlink;remediation=reject")
        self.assertEqual(outside.read_text(encoding="utf-8"), "do not touch\n")

    def test_linked_worktree_resolves_to_the_wrapper_root(self) -> None:
        repository = Path(__file__).resolve().parents[1]
        root = resolve_shared_root(repository)
        common = Path(
            subprocess.check_output(
                ["git", "-C", str(repository), "rev-parse", "--git-common-dir"],
                text=True,
            ).strip()
        )
        if not common.is_absolute():
            common = repository / common
        self.assertEqual(root, common.resolve().parent)


if __name__ == "__main__":
    unittest.main()
