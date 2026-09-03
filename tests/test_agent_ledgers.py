from __future__ import annotations

from concurrent.futures import ThreadPoolExecutor
import errno
import io
import os
from pathlib import Path
import subprocess
import tempfile
from types import SimpleNamespace
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

    def test_validation_rejects_bad_inputs_and_mapping_shapes(self) -> None:
        with self.assertRaisesRegex(AgentLedgerError, "process-improvements or"):
            agent_ledgers._spec("unsupported")
        with self.assertRaisesRegex(AgentLedgerError, "lowercase SHA-256"):
            agent_ledgers._validate_expected_digest("not-a-digest")
        with self.assertRaisesRegex(AgentLedgerError, "must be text"):
            agent_ledgers._field(None, None)  # type: ignore[arg-type]
        with self.assertRaisesRegex(AgentLedgerError, "must not be empty"):
            agent_ledgers._field("value", " ")
        with self.assertRaisesRegex(AgentLedgerError, "without pipes"):
            agent_ledgers._field("value", "bad | value")
        with self.assertRaisesRegex(AgentLedgerError, "without pipes"):
            agent_ledgers._field("value", "bad\nvalue")
        with self.assertRaisesRegex(AgentLedgerError, "control character"):
            agent_ledgers._field("value", "bad\x01value")
        with self.assertRaisesRegex(AgentLedgerError, "backticks"):
            agent_ledgers._field("key", "bad`key", key=True)
        self.assertRegex(agent_ledgers._timestamp(None), r"Z$")
        with self.assertRaisesRegex(AgentLedgerError, "valid UTC timestamp"):
            agent_ledgers._timestamp("2026-09-03")

        with self.assertRaisesRegex(
            AgentLedgerError, "process-improvement stable key"
        ):
            self._process("bad key")
        with self.assertRaisesRegex(AgentLedgerError, "process-improvement status"):
            update_agent_ledger(
                self.root,
                "process-improvements",
                key="valid-key",
                status="invalid",
                observation="observed",
                expected_benefit="bounded benefit",
            )
        with self.assertRaisesRegex(AgentLedgerError, "tooling-issue status"):
            update_agent_ledger(
                self.root,
                "tooling-issues",
                key="mechanism=valid;remediation=status",
                status="invalid",
                observation="observed",
                impact="bounded impact",
                recommended_action="retry safely",
            )
        with self.assertRaisesRegex(AgentLedgerError, "unsupported row fields"):
            agent_ledgers.update_from_mapping(
                self.root,
                "tooling-issues",
                {
                    "key": "mechanism=mapping;remediation=unknown",
                    "status": "open",
                    "observation": "observed",
                    "unexpected": "reject",
                },
            )
        with self.assertRaisesRegex(AgentLedgerError, "missing required fields"):
            agent_ledgers.update_from_mapping(
                self.root,
                "tooling-issues",
                {"key": "mechanism=mapping;remediation=missing", "status": "open"},
            )
        result = agent_ledgers.update_from_mapping(
            self.root,
            "tooling-issues",
            {
                "key": "mechanism=mapping;remediation=valid",
                "status": "open",
                "observation": "observed",
                "impact": "bounded impact",
                "recommended_action": "retry safely",
            },
        )
        self.assertEqual(result["operation"], "added")

    def test_candidate_and_table_validation_rejects_bad_bytes(self) -> None:
        spec = agent_ledgers._spec("tooling-issues")
        with self.assertRaisesRegex(AgentLedgerError, "exceeds the 128 KiB"):
            agent_ledgers._validate_candidate(
                spec, b"x" * (agent_ledgers.AGENT_LEDGER_MAX_BYTES + 1)
            )
        with self.assertRaisesRegex(AgentLedgerError, "not UTF-8"):
            agent_ledgers._validate_candidate(spec, b"\xff")
        with mock.patch.object(
            agent_ledgers, "validate_tooling_ledger_text", return_value=["invalid"]
        ):
            with self.assertRaisesRegex(AgentLedgerError, "candidate rejected"):
                agent_ledgers._validate_candidate(spec, b"candidate")

        header = "| Stable key | Status | Observation | Impact | Recommended action |"
        separator = "| --- | --- | --- | --- | --- |"
        row = "| `mechanism=table;remediation=valid` | open | observed | impact | action |"
        with self.assertRaisesRegex(AgentLedgerError, "table is not canonical"):
            agent_ledgers._table_for(spec, "# Agent tooling issues\n")
        with self.assertRaisesRegex(AgentLedgerError, "separator is invalid"):
            agent_ledgers._table_for(spec, f"{header}\n| bad |\n{row}\n")
        with self.assertRaisesRegex(AgentLedgerError, "malformed row"):
            agent_ledgers._table_for(spec, f"{header}\n{separator}\n| bad |\n")
        with self.assertRaisesRegex(AgentLedgerError, "duplicate stable keys"):
            agent_ledgers._table_for(spec, f"{header}\n{separator}\n{row}\n{row}\n")
        with self.assertRaisesRegex(AgentLedgerError, "contains no rows"):
            agent_ledgers._table_for(spec, f"{header}\n{separator}\n")
        table = agent_ledgers._table_for(
            spec, f"{header}\n{separator}\n{row}\n\nnotes\n"
        )
        self.assertEqual(table.insert_at, 3)
        with self.assertRaisesRegex(AgentLedgerError, "not UTF-8"):
            agent_ledgers._merge(
                spec,
                b"\xff",
                ("mechanism=x;remediation=y", "open", "o", "i", "a"),
            )
        no_final_newline = f"# Agent tooling issues\n\n{header}\n{separator}\n{row}"
        merged, operation = agent_ledgers._merge(
            spec,
            no_final_newline.encode("utf-8"),
            ("mechanism=table;remediation=added", "open", "o", "i", "a"),
        )
        self.assertEqual(operation, "added")
        self.assertTrue(merged.endswith(b"\n"))

    @unittest.skipIf(
        os.name == "nt", "native Windows has different descriptor semantics"
    )
    def test_snapshot_and_directory_guards_fail_closed(self) -> None:
        build = self.root / "build"
        build.mkdir()
        not_a_directory = self.root / "not-a-directory"
        not_a_directory.write_text("file", encoding="utf-8")
        with self.assertRaisesRegex(
            AgentLedgerError, "shared build directory is unsafe"
        ):
            with agent_ledgers._opened_build_directory(not_a_directory):
                pass
        with agent_ledgers._opened_build_directory(build) as directory:
            path = build / "not-a-file"
            path.mkdir()
            with self.assertRaisesRegex(AgentLedgerError, "not a regular file"):
                agent_ledgers._read_snapshot(path, directory)
            with mock.patch.object(
                agent_ledgers.os,
                "open",
                side_effect=OSError(errno.ENOENT, "missing"),
            ):
                self.assertIsNone(
                    agent_ledgers._read_snapshot(build / "missing", directory).data
                )
        self.assertEqual(agent_ledgers._read_limited(io.BytesIO(b"bytes")), b"bytes")
        with self.assertRaisesRegex(AgentLedgerError, "exceeds the 128 KiB"):
            agent_ledgers._read_limited(
                io.BytesIO(b"x" * (agent_ledgers.AGENT_LEDGER_MAX_BYTES + 1))
            )

    @unittest.skipIf(
        os.name == "nt", "native Windows has different symlink semantics"
    )
    def test_repository_and_local_only_discovery_errors(self) -> None:
        with mock.patch.object(
            agent_ledgers.subprocess, "run", side_effect=OSError("git unavailable")
        ):
            with self.assertRaisesRegex(AgentLedgerError, "cannot discover"):
                agent_ledgers._git_root_output(self.root, "--show-toplevel")
        with mock.patch.object(
            agent_ledgers.subprocess,
            "run",
            return_value=SimpleNamespace(returncode=1, stdout=""),
        ):
            with self.assertRaisesRegex(AgentLedgerError, "not a Git checkout"):
                agent_ledgers._git_root_output(self.root, "--show-toplevel")
        file_path = self.root / "file"
        file_path.write_text("file", encoding="utf-8")
        with self.assertRaisesRegex(AgentLedgerError, "not a directory"):
            agent_ledgers.resolve_shared_root(file_path)
        with self.assertRaisesRegex(
            AgentLedgerError, "shared root is not a directory"
        ):
            agent_ledgers._canonical_root(file_path)
        outside = self.root / "outside"
        outside.mkdir()
        (self.root / "build").symlink_to(outside, target_is_directory=True)
        with self.assertRaisesRegex(
            AgentLedgerError, "establish the shared build directory"
        ):
            agent_ledgers._ensure_build_directory(self.root)

        spec = agent_ledgers._spec("tooling-issues")
        with mock.patch.object(
            agent_ledgers.subprocess, "run", side_effect=OSError("check failed")
        ):
            with self.assertRaisesRegex(AgentLedgerError, "cannot verify"):
                agent_ledgers._verify_local_only(self.root, spec)
        with mock.patch.object(
            agent_ledgers.subprocess,
            "run",
            side_effect=[SimpleNamespace(returncode=0), OSError("tracking failed")],
        ):
            with self.assertRaisesRegex(AgentLedgerError, "tracking state"):
                agent_ledgers._verify_local_only(self.root, spec)
        with mock.patch.object(
            agent_ledgers.subprocess,
            "run",
            side_effect=[SimpleNamespace(returncode=0), SimpleNamespace(returncode=2)],
        ):
            with self.assertRaisesRegex(AgentLedgerError, "cannot verify tracking"):
                agent_ledgers._verify_local_only(self.root, spec)

    def test_publication_and_update_recovery_edges(self) -> None:
        build = self.root / "build"
        build.mkdir()
        path = build / "agent-tooling-issues.md"
        with agent_ledgers._opened_build_directory(build) as directory:
            with mock.patch.object(
                agent_ledgers,
                "_assert_directory_identity",
                side_effect=[None, AgentLedgerError("verification failed")],
            ):
                with self.assertRaises(AgentLedgerCommitUncertain):
                    agent_ledgers._atomic_publish(path, directory, b"published\n")
            self.assertEqual(path.read_bytes(), b"published\n")
        with self.assertRaisesRegex(AgentLedgerError, "missing shared directory"):
            agent_ledgers._atomic_publish(path, None, b"data")
        for failure in (FileNotFoundError(), OSError("cleanup failed")):
            with agent_ledgers._opened_build_directory(build) as directory:
                with mock.patch.object(
                    agent_ledgers, "flush_file", side_effect=OSError("flush failed")
                ), mock.patch.object(agent_ledgers.os, "unlink", side_effect=failure):
                    with self.assertRaises(OSError):
                        agent_ledgers._atomic_publish(path, directory, b"retry\n")

        with mock.patch.object(
            agent_ledgers, "ledger_lock_path", return_value=ledger_path(self.root, "tooling-issues")
        ):
            with self.assertRaisesRegex(AgentLedgerError, "replaceable ledger"):
                self._tooling("mechanism=lock;remediation=reject")
        with mock.patch.object(
            agent_ledgers,
            "_read_snapshot",
            return_value=agent_ledgers._Snapshot(
                b"x" * (agent_ledgers.AGENT_LEDGER_MAX_BYTES + 1)
            ),
        ):
            with self.assertRaisesRegex(AgentLedgerError, "current ledger exceeds"):
                self._tooling("mechanism=oversized;remediation=reject")
        with mock.patch.object(
            agent_ledgers, "_read_snapshot", return_value=agent_ledgers._Snapshot(b"\xff")
        ):
            with self.assertRaisesRegex(
                AgentLedgerError, "current ledger is not UTF-8"
            ):
                self._tooling("mechanism=encoding;remediation=reject")
        with mock.patch.object(
            agent_ledgers,
            "_read_snapshot",
            side_effect=[
                agent_ledgers._Snapshot(None),
                agent_ledgers._Snapshot(b"changed"),
            ],
        ):
            with self.assertRaisesRegex(AgentLedgerError, "changed while preparing"):
                self._tooling("mechanism=latest;remediation=changed")

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
