"""Executable, no-network model tests for the program publication contract."""

from __future__ import annotations

import copy
import unittest


class StopClosed(RuntimeError):
    """The modeled ledger cannot safely authorize a remote mutation."""


class ProgramLedgerModel:
    """Small reference reducer used to exercise crash/retry invariants."""

    def __init__(self) -> None:
        self.generation = 0
        self.comment = {"phase": "none", "node": None, "calls": 0}
        self.create = {"phase": "none", "node": None, "calls": 0}
        self.link = {"phase": "none", "node": None, "calls": 0}
        self.graph = "old"
        self.authority = ("repo", "master", "goal", "actor")
        self.parent = "master-node"
        self.lock_inode = 41

    @staticmethod
    def initialize(lock_exists: bool, ledger_exists: bool) -> "ProgramLedgerModel":
        if lock_exists or ledger_exists:
            raise StopClosed("not a provably fresh coordinate")
        return ProgramLedgerModel()

    @staticmethod
    def stable_scan(
        cursors: list[str], node_ids: list[str], first_digest: str,
        second_digest: str, complete: bool = True,
    ) -> None:
        if (
            not complete
            or len(cursors) != len(set(cursors))
            or len(node_ids) != len(set(node_ids))
            or first_digest != second_digest
        ):
            raise StopClosed("pagination is incomplete or changed")

    def snapshot(self) -> dict[str, object]:
        return copy.deepcopy(self.__dict__)

    def cas(self, expected_generation: int, expected_lock_inode: int = 41) -> None:
        if expected_generation != self.generation:
            raise StopClosed("stale generation")
        if expected_lock_inode != self.lock_inode:
            raise StopClosed("substituted lock")
        self.generation += 1

    def plan_comment(self, namespace_occurrences: int) -> None:
        if self.comment["phase"] != "none" or namespace_occurrences != 0:
            raise StopClosed("comment namespace is not fresh")
        self.comment["phase"] = "planned"

    def call_comment(self) -> None:
        if self.comment["phase"] != "planned":
            raise StopClosed("comment call lacks planned intent")
        self.comment["phase"] = "in-flight"
        self.comment["calls"] += 1

    def reconcile_comment(self, exact_nodes: list[str]) -> None:
        if self.comment["phase"] != "in-flight" or len(exact_nodes) != 1:
            raise StopClosed("comment result is uncertain")
        self.comment.update(phase="bound", node=exact_nodes[0])

    def rekey(self, authority: tuple[str, ...], graph: str, node: str) -> None:
        if authority != self.authority or self.comment != {
            "phase": "bound", "node": node, "calls": 1
        }:
            raise StopClosed("rekey changes authority or node")
        self.graph = graph

    def plan_create(self, proven_missing: bool, candidates: int) -> None:
        if self.create["phase"] != "none" or not proven_missing or candidates:
            raise StopClosed("child is not proven missing")
        self.create["phase"] = "planned"

    def call_create(self) -> None:
        if self.create["phase"] != "planned":
            raise StopClosed("create call lacks planned intent")
        self.create["phase"] = "in-flight"
        self.create["calls"] += 1

    def reconcile_create(self, exact_nodes: list[str]) -> None:
        if self.create["phase"] != "in-flight" or len(exact_nodes) != 1:
            raise StopClosed("create result is uncertain")
        self.create.update(phase="bound", node=exact_nodes[0])

    def plan_link(self, relationships: list[str], parent: str = "master-node") -> None:
        if self.create["phase"] != "bound" or relationships or parent != self.parent:
            raise StopClosed("parent graph is not linkable")
        self.link["phase"] = "planned"

    def call_link(self) -> None:
        if self.link["phase"] != "planned":
            raise StopClosed("link call lacks planned intent")
        self.link["phase"] = "in-flight"
        self.link["calls"] += 1

    def reconcile_link(self, exact_relationships: list[str]) -> None:
        if self.link["phase"] != "in-flight" or len(exact_relationships) != 1:
            raise StopClosed("link result is uncertain")
        self.link.update(phase="bound", node=exact_relationships[0])


class ProgramLedgerModelTests(unittest.TestCase):
    def test_fresh_and_lock_only_initialization_are_distinct(self) -> None:
        self.assertIsInstance(
            ProgramLedgerModel.initialize(False, False), ProgramLedgerModel
        )
        for lock, ledger in ((True, False), (False, True), (True, True)):
            with self.subTest(lock=lock, ledger=ledger):
                with self.assertRaises(StopClosed):
                    ProgramLedgerModel.initialize(lock, ledger)

    def test_multi_page_scan_requires_unique_stable_complete_stream(self) -> None:
        ProgramLedgerModel.stable_scan(["a", "b"], ["1", "2"], "d", "d")
        cases = (
            (["a"], ["1"], "d", "d", False),
            (["a", "a"], ["1", "2"], "d", "d", True),
            (["a", "b"], ["1", "1"], "d", "d", True),
            (["a"], ["1"], "before", "after", True),
        )
        for args in cases:
            with self.subTest(args=args):
                with self.assertRaises(StopClosed):
                    ProgramLedgerModel.stable_scan(*args)

    def test_crash_before_calls_is_resumable_once(self) -> None:
        model = ProgramLedgerModel()
        model.plan_comment(0)
        resumed = copy.deepcopy(model)
        resumed.call_comment()
        resumed.reconcile_comment(["comment-node"])
        self.assertEqual(resumed.comment["calls"], 1)

    def test_accepted_but_invisible_comment_never_reposts(self) -> None:
        model = ProgramLedgerModel()
        model.plan_comment(0)
        model.call_comment()
        with self.assertRaises(StopClosed):
            model.reconcile_comment([])
        with self.assertRaises(StopClosed):
            model.call_comment()
        self.assertEqual(model.comment["calls"], 1)

    def test_any_old_namespace_marker_blocks_fresh_post(self) -> None:
        with self.assertRaises(StopClosed):
            ProgramLedgerModel().plan_comment(1)

    def test_concurrent_or_substituted_writer_fails_cas(self) -> None:
        model = ProgramLedgerModel()
        model.cas(0)
        for generation, inode in ((0, 41), (1, 99)):
            with self.subTest(generation=generation, inode=inode):
                with self.assertRaises(StopClosed):
                    model.cas(generation, inode)

    def test_rekey_retains_authority_and_comment_node(self) -> None:
        model = ProgramLedgerModel()
        model.plan_comment(0)
        model.call_comment()
        model.reconcile_comment(["comment-node"])
        model.rekey(model.authority, "new", "comment-node")
        self.assertEqual(model.comment["node"], "comment-node")
        with self.assertRaises(StopClosed):
            model.rekey(("other",) * 4, "newer", "comment-node")

    def test_duplicate_or_incomplete_search_blocks_create(self) -> None:
        for missing, candidates in ((False, 0), (True, 1)):
            with self.subTest(missing=missing, candidates=candidates):
                with self.assertRaises(StopClosed):
                    ProgramLedgerModel().plan_create(missing, candidates)

    def test_uncertain_create_never_reposts_and_can_later_bind(self) -> None:
        model = ProgramLedgerModel()
        model.plan_create(True, 0)
        model.call_create()
        with self.assertRaises(StopClosed):
            model.reconcile_create([])
        with self.assertRaises(StopClosed):
            model.call_create()
        model.reconcile_create(["issue-node"])
        self.assertEqual(model.create["calls"], 1)

    def test_link_requires_bound_child_and_empty_parent_graph(self) -> None:
        model = ProgramLedgerModel()
        with self.assertRaises(StopClosed):
            model.plan_link([])
        model.create.update(phase="bound", node="issue-node")
        with self.assertRaises(StopClosed):
            model.plan_link(["existing-link"])
        with self.assertRaises(StopClosed):
            model.plan_link([], parent="wrong-parent")

    def test_uncertain_link_never_reposts_and_can_later_bind(self) -> None:
        model = ProgramLedgerModel()
        model.create.update(phase="bound", node="issue-node")
        model.plan_link([])
        model.call_link()
        with self.assertRaises(StopClosed):
            model.reconcile_link([])
        with self.assertRaises(StopClosed):
            model.call_link()
        model.reconcile_link(["relationship-node"])
        self.assertEqual(model.link["calls"], 1)

    def test_duplicate_results_stop_instead_of_binding(self) -> None:
        model = ProgramLedgerModel()
        model.plan_create(True, 0)
        model.call_create()
        with self.assertRaises(StopClosed):
            model.reconcile_create(["one", "two"])


if __name__ == "__main__":
    unittest.main()
