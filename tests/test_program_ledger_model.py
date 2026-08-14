"""Executable, no-network model tests for the program publication contract."""

from __future__ import annotations

import copy
import fcntl
import hashlib
import json
import os
from pathlib import Path
import tempfile
import unittest


class StopClosed(RuntimeError):
    """The modeled ledger cannot safely authorize a remote mutation."""


class CallPermit:
    """Ephemeral authority returned only by the durable in-flight transition."""

    def __init__(self, slot: str, generation: int) -> None:
        self.slot = slot
        self.generation = generation
        self.used = False


class ProgramLedgerModel:
    """Reference reducer with distinct persistence and remote-call boundaries."""

    KEYS = {
        "generation", "self_inode", "previous_sha256", "authority", "graph",
        "next_graph", "comment", "create", "link", "leaf_snapshots",
    }

    def __init__(self) -> None:
        empty = {"phase": "none", "node": None, "prior": None}
        self.record: dict[str, object] = {
            "generation": 0,
            "self_inode": 101,
            "previous_sha256": None,
            "authority": ["repo", "master", "goal", "actor"],
            "graph": ["leaf-1"],
            "next_graph": None,
            "comment": copy.deepcopy(empty),
            "create": copy.deepcopy(empty),
            "link": copy.deepcopy(empty),
            "leaf_snapshots": {"leaf-1": [1, "a" * 64]},
        }
        self.lock_inode = 41
        self.remote_calls = {"comment": 0, "create": 0, "link": 0}

    @staticmethod
    def canonical(record: dict[str, object]) -> bytes:
        return (json.dumps(record, sort_keys=True, separators=(",", ":")) + "\n").encode()

    def digest(self) -> str:
        return hashlib.sha256(self.canonical(self.record)).hexdigest()

    @classmethod
    def fresh(cls, lock_exists: bool, ledger_exists: bool) -> "ProgramLedgerModel":
        if lock_exists or ledger_exists:
            raise StopClosed("not a provably fresh coordinate")
        return cls()

    @classmethod
    def resume(
        cls, record: dict[str, object] | None, lock_inode: int | None,
        observed_inode: int | None, observed_sha256: str | None,
    ) -> "ProgramLedgerModel":
        if record is None or lock_inode is None:
            raise StopClosed("ledger or stable lock was lost")
        if set(record) != cls.KEYS or observed_inode != record.get("self_inode"):
            raise StopClosed("schema or inode corruption")
        if hashlib.sha256(cls.canonical(record)).hexdigest() != observed_sha256:
            raise StopClosed("byte corruption")
        model = cls()
        model.record = copy.deepcopy(record)
        model.lock_inode = lock_inode
        return model

    def persist(
        self, mutate: object, expected_generation: int | None = None,
        expected_digest: str | None = None, expected_lock_inode: int = 41,
    ) -> None:
        generation = int(self.record["generation"])
        old_digest = self.digest()
        if expected_generation is not None and expected_generation != generation:
            raise StopClosed("stale generation")
        if expected_digest is not None and expected_digest != old_digest:
            raise StopClosed("stale digest")
        if expected_lock_inode != self.lock_inode:
            raise StopClosed("substituted lock")
        mutate(self.record)
        self.record["previous_sha256"] = old_digest
        self.record["generation"] = generation + 1
        self.record["self_inode"] = int(self.record["self_inode"]) + 1

    @staticmethod
    def stable_scan(
        cursors: list[str], node_ids: list[str], first_digest: str,
        second_digest: str, complete: bool = True, pages: int = 1,
        nodes: int = 0, body_bytes: int = 0, timed_out: bool = False,
    ) -> None:
        if (
            not complete or timed_out or pages > 100 or nodes > 10_000
            or body_bytes > 16 * 1024 * 1024
            or len(cursors) != len(set(cursors))
            or len(node_ids) != len(set(node_ids))
            or first_digest != second_digest
        ):
            raise StopClosed("pagination is incomplete, excessive, or changed")

    @staticmethod
    def validate_marker(namespace: list[tuple[str, str]], actor: str, bound: bool) -> None:
        if not bound and namespace:
            raise StopClosed("fresh namespace is not empty")
        if bound and (len(namespace) != 1 or namespace[0][1] != actor):
            raise StopClosed("marker is missing, duplicate, or wrong-author")

    def plan(self, slot: str, node: str | None = None, prior: str | None = None) -> None:
        current = self.record[slot]
        if current["phase"] not in {"none", "bound"}:
            raise StopClosed("slot already owns an intent")
        self.persist(lambda r: r[slot].update(phase="planned", node=node, prior=prior))

    def arm(self, slot: str) -> CallPermit:
        if self.record[slot]["phase"] != "planned":
            raise StopClosed("remote call lacks planned intent")
        self.persist(lambda r: r[slot].update(phase="in-flight"))
        return CallPermit(slot, int(self.record["generation"]))

    def execute(self, permit: CallPermit) -> None:
        if (
            permit.used or permit.generation != self.record["generation"]
            or self.record[permit.slot]["phase"] != "in-flight"
        ):
            raise StopClosed("call permit is absent, stale, or already used")
        permit.used = True
        self.remote_calls[permit.slot] += 1

    def bind(self, slot: str, exact_results: list[str]) -> None:
        if self.record[slot]["phase"] != "in-flight" or len(exact_results) != 1:
            raise StopClosed("remote result is uncertain")
        self.persist(
            lambda r: r[slot].update(
                phase="bound", node=exact_results[0], prior=None
            )
        )

    def bind_create(self, candidates: list[dict[str, str]], boundary: str) -> None:
        exact = [
            item for item in candidates
            if set(item) == {"node", "creator", "title", "body", "created_at"}
            and item["node"] == "issue-node" and item["creator"] == "actor"
            and item["title"] == "title" and item["body"] == "body"
            and item["created_at"] >= boundary
        ]
        self.bind("create", [item["node"] for item in exact])

    def bind_link(
        self, child_parent: str, parent_subissues: list[str], stream_digest: str,
    ) -> None:
        if child_parent != "master" or parent_subissues.count("issue-node") != 1:
            raise StopClosed("parent-child pair is not proven in both directions")
        if self.record["link"]["phase"] != "in-flight":
            raise StopClosed("link result lacks durable intent")
        self.persist(
            lambda record: record["link"].update(
                phase="bound", node=None, prior=None, parent="master",
                child="issue-node", proof=stream_digest,
            )
        )

    def rekey(self, authority: list[str], next_graph: list[str], node: str) -> None:
        if (
            authority != self.record["authority"]
            or self.record["comment"]["phase"] != "bound"
            or self.record["comment"]["node"] != node
        ):
            raise StopClosed("rekey changes authority or comment node")
        self.persist(lambda r: r.update(next_graph=copy.deepcopy(next_graph)))
        self.plan("comment", node=node, prior="old-body")

    def compose_leaf(self, position: str, generation: int, digest: str) -> None:
        snapshot = self.record["leaf_snapshots"].get(position)
        if snapshot != [generation, digest] or position not in self.record["graph"]:
            raise StopClosed("leaf ownership or evidence does not match graph")


class ProgramLedgerModelTests(unittest.TestCase):
    def test_fresh_and_persisted_resume_are_distinct(self) -> None:
        model = ProgramLedgerModel.fresh(False, False)
        resumed = ProgramLedgerModel.resume(
            model.record, model.lock_inode, model.record["self_inode"], model.digest()
        )
        self.assertEqual(resumed.record, model.record)
        for args in ((None, 41, 101, None), (model.record, None, 101, model.digest())):
            with self.subTest(args=args):
                with self.assertRaises(StopClosed):
                    ProgramLedgerModel.resume(*args)

    def test_corrupt_lost_or_inode_substituted_ledger_stops(self) -> None:
        model = ProgramLedgerModel()
        corrupt = copy.deepcopy(model.record)
        corrupt["unknown"] = True
        cases = (
            (corrupt, 41, 101, model.digest()),
            (model.record, 41, 999, model.digest()),
            (model.record, 41, 101, "0" * 64),
        )
        for args in cases:
            with self.subTest(args=args):
                with self.assertRaises(StopClosed):
                    ProgramLedgerModel.resume(*args)

    def test_filesystem_lock_is_stable_across_atomic_ledger_replace(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            lock = root / "ledger.lock"
            ledger = root / "ledger.json"
            lock.touch(mode=0o600)
            with lock.open("r+") as descriptor:
                fcntl.flock(descriptor, fcntl.LOCK_EX | fcntl.LOCK_NB)
                lock_inode = os.fstat(descriptor.fileno()).st_ino
                ledger.write_text("old", encoding="utf-8")
                temporary = root / "ledger.tmp"
                temporary.write_text("new", encoding="utf-8")
                os.replace(temporary, ledger)
                self.assertEqual(os.stat(lock).st_ino, lock_inode)

    def test_multi_page_bounds_and_stream_stability(self) -> None:
        ProgramLedgerModel.stable_scan(["a", "b"], ["1", "2"], "d", "d")
        cases = (
            {"complete": False}, {"timed_out": True}, {"pages": 101},
            {"nodes": 10_001}, {"body_bytes": 16 * 1024 * 1024 + 1},
        )
        for kwargs in cases:
            with self.subTest(kwargs=kwargs), self.assertRaises(StopClosed):
                ProgramLedgerModel.stable_scan([], [], "d", "d", **kwargs)
        with self.assertRaises(StopClosed):
            ProgramLedgerModel.stable_scan(["a", "a"], ["1"], "d", "d")
        with self.assertRaises(StopClosed):
            ProgramLedgerModel.stable_scan([], [], "before", "after")

    def test_marker_namespace_actor_and_duplicates_fail_closed(self) -> None:
        ProgramLedgerModel.validate_marker([], "actor", False)
        ProgramLedgerModel.validate_marker([("marker", "actor")], "actor", True)
        for namespace, bound in (([("old", "actor")], False), ([], True),
                                 ([("a", "actor"), ("b", "actor")], True),
                                 ([("a", "other")], True)):
            with self.subTest(namespace=namespace, bound=bound):
                with self.assertRaises(StopClosed):
                    ProgramLedgerModel.validate_marker(namespace, "actor", bound)

    def test_crash_before_remote_call_loses_permit_and_never_calls(self) -> None:
        model = ProgramLedgerModel()
        model.plan("comment")
        model.arm("comment")
        resumed = ProgramLedgerModel.resume(
            model.record, 41, model.record["self_inode"], model.digest()
        )
        with self.assertRaises(StopClosed):
            resumed.arm("comment")
        self.assertEqual(resumed.remote_calls["comment"], 0)

    def test_crash_after_each_remote_call_never_reposts(self) -> None:
        for slot in ("comment", "create", "link"):
            with self.subTest(slot=slot):
                model = ProgramLedgerModel()
                model.plan(slot)
                permit = model.arm(slot)
                model.execute(permit)
                with self.assertRaises(StopClosed):
                    model.execute(permit)
                resumed = ProgramLedgerModel.resume(
                    model.record, 41, model.record["self_inode"], model.digest()
                )
                with self.assertRaises(StopClosed):
                    resumed.arm(slot)
                self.assertEqual(model.remote_calls[slot], 1)

    def test_stale_generation_digest_and_lock_writers_stop(self) -> None:
        model = ProgramLedgerModel()
        generation, digest = model.record["generation"], model.digest()
        model.persist(lambda record: None, generation, digest)
        for args in ((generation, model.digest(), 41),
                     (model.record["generation"], digest, 41),
                     (model.record["generation"], model.digest(), 99)):
            with self.subTest(args=args), self.assertRaises(StopClosed):
                model.persist(lambda record: None, *args)

    def test_patch_and_rekey_keep_one_comment_node_and_authority(self) -> None:
        model = ProgramLedgerModel()
        model.plan("comment")
        permit = model.arm("comment")
        model.execute(permit)
        model.bind("comment", ["comment-node"])
        model.rekey(model.record["authority"], ["leaf-1", "leaf-2"], "comment-node")
        patch = model.arm("comment")
        model.execute(patch)
        model.bind("comment", ["comment-node"])
        self.assertEqual(model.record["comment"]["node"], "comment-node")
        with self.assertRaises(StopClosed):
            model.rekey(["other"] * 4, ["bad"], "comment-node")

    def test_create_result_requires_exact_actor_bytes_and_time_boundary(self) -> None:
        model = ProgramLedgerModel()
        model.plan("create")
        permit = model.arm("create")
        model.execute(permit)
        boundary = "2026-08-14T00:00:00Z"
        wrong = {"node": "issue-node", "creator": "other", "title": "title",
                 "body": "body", "created_at": boundary}
        with self.assertRaises(StopClosed):
            model.bind_create([wrong], boundary)
        exact = dict(wrong, creator="actor")
        model.bind_create([exact], boundary)

    def test_native_link_binds_parent_child_pair_without_edge_node(self) -> None:
        model = ProgramLedgerModel()
        model.plan("link")
        permit = model.arm("link")
        model.execute(permit)
        with self.assertRaises(StopClosed):
            model.bind_link("wrong-parent", ["issue-node"], "digest")
        model.bind_link("master", ["issue-node"], "digest")
        self.assertEqual(
            (model.record["link"]["parent"], model.record["link"]["child"]),
            ("master", "issue-node"),
        )

    def test_duplicate_uncertain_results_stop_without_repost(self) -> None:
        model = ProgramLedgerModel()
        model.plan("create")
        permit = model.arm("create")
        model.execute(permit)
        for results in ([], ["one", "two"]):
            with self.subTest(results=results), self.assertRaises(StopClosed):
                model.bind("create", results)
        self.assertEqual(model.remote_calls["create"], 1)

    def test_leaf_composition_rejects_overlap_reorder_and_drift(self) -> None:
        model = ProgramLedgerModel()
        model.compose_leaf("leaf-1", 1, "a" * 64)
        for args in (("leaf-2", 1, "a" * 64), ("leaf-1", 2, "a" * 64),
                     ("leaf-1", 1, "b" * 64)):
            with self.subTest(args=args), self.assertRaises(StopClosed):
                model.compose_leaf(*args)


if __name__ == "__main__":
    unittest.main()
