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
        "generation", "self_inode", "lock", "previous_sha256", "authority", "graph",
        "next_graph", "comment", "create", "link", "leaf_snapshots",
    }

    def __init__(self) -> None:
        empty = {"phase": "none", "node": None, "prior": None}
        self.record: dict[str, object] = {
            "generation": 0,
            "self_inode": 101,
            "lock": {"device": 1, "inode": 41},
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
        self.path_lock_inode = 41
        self.remote_calls = {"comment": 0, "create": 0, "link": 0}
        self.report_present = False

    @staticmethod
    def canonical(record: dict[str, object]) -> bytes:
        return (json.dumps(record, sort_keys=True, separators=(",", ":")) + "\n").encode()

    def digest(self) -> str:
        return hashlib.sha256(self.canonical(self.record)).hexdigest()

    @staticmethod
    def coordinate(repository_node: str, master_node: str) -> str:
        payload = {"master_node_id": master_node, "repository_node_id": repository_node}
        return hashlib.sha256(ProgramLedgerModel.canonical(payload)).hexdigest()

    @classmethod
    def fresh(cls, lock_exists: bool, ledger_exists: bool) -> "ProgramLedgerModel":
        if lock_exists or ledger_exists:
            raise StopClosed("not a provably fresh coordinate")
        return cls()

    @classmethod
    def resume(
        cls, record: dict[str, object] | None, lock_inode: int | None,
        observed_inode: int | None, observed_sha256: str | None,
        expected_authority: list[str], expected_previous: str | None = None,
        report_present: bool = False,
        remote_calls: dict[str, int] | None = None,
    ) -> "ProgramLedgerModel":
        if record is None or lock_inode is None:
            raise StopClosed("ledger or stable lock was lost")
        if set(record) != cls.KEYS or observed_inode != record.get("self_inode"):
            raise StopClosed("schema or inode corruption")
        if record["lock"] != {"device": 1, "inode": lock_inode}:
            raise StopClosed("arbitration lock identity changed")
        if hashlib.sha256(cls.canonical(record)).hexdigest() != observed_sha256:
            raise StopClosed("byte corruption")
        if record["authority"] != expected_authority:
            raise StopClosed("live authority changed")
        generation = record["generation"]
        previous = record["previous_sha256"]
        if (
            not isinstance(generation, int) or isinstance(generation, bool)
            or generation < 0 or (generation == 0 and previous is not None)
            or (generation > 0 and (
                not isinstance(previous, str) or len(previous) != 64
            ))
            or (expected_previous is not None and previous != expected_previous)
        ):
            raise StopClosed("generation lineage is corrupt")
        common_keys = {"phase", "node", "prior"}
        for name in ("comment", "create", "link"):
            slot = record[name]
            if not isinstance(slot, dict) or slot.get("phase") not in {
                "none", "planned", "in-flight", "bound"
            }:
                raise StopClosed("slot phase is corrupt")
            expected_keys = (
                common_keys | {"parent", "child", "proof"}
                if name == "link" and slot["phase"] == "bound"
                else common_keys
            )
            if set(slot) != expected_keys:
                raise StopClosed("slot keys are corrupt")
            if slot["phase"] == "none" and (
                slot.get("node") is not None or slot.get("prior") is not None
            ):
                raise StopClosed("none phase contains result state")
            if name in {"comment", "create"} and slot["phase"] == "bound" and (
                not isinstance(slot["node"], str) or slot["prior"] is not None
            ):
                raise StopClosed("bound result is incomplete")
            if name == "create" and slot["phase"] in {"planned", "in-flight"} and (
                slot["node"] is not None or slot["prior"] is not None
            ):
                raise StopClosed("create intent contains a result")
            if name == "link" and slot["phase"] in {"planned", "in-flight"} and (
                slot["node"] is not None or slot["prior"] is not None
            ):
                raise StopClosed("link intent contains a result")
            if name == "comment" and slot["phase"] in {"planned", "in-flight"} and not (
                (slot["node"] is None and slot["prior"] is None)
                or (isinstance(slot["node"], str) and slot["prior"] == "old-body")
            ):
                raise StopClosed("comment POST/PATCH intent is corrupt")
            if name == "link" and slot["phase"] == "bound" and (
                slot["node"] is not None or slot["prior"] is not None
                or slot["parent"] != "master" or slot["child"] != "issue-node"
                or not isinstance(slot["proof"], str) or not slot["proof"]
            ):
                raise StopClosed("bound link proof is incomplete")
        if record["link"]["phase"] != "none" and not (
            record["create"]["phase"] == "bound"
            and record["create"]["node"] == "issue-node"
        ):
            raise StopClosed("link exists without its bound child")
        if record["next_graph"] is not None and not (
            record["comment"]["phase"] in {"planned", "in-flight"}
            and record["comment"]["node"] is not None
            and record["comment"]["prior"] == "old-body"
        ):
            raise StopClosed("next graph lacks its PATCH intent")
        if not set(record["leaf_snapshots"]).issubset(set(record["graph"])):
            raise StopClosed("leaf snapshot is outside the graph")
        model = cls()
        model.record = copy.deepcopy(record)
        model.lock_inode = lock_inode
        model.path_lock_inode = lock_inode
        model.report_present = report_present
        if remote_calls is not None:
            model.remote_calls = remote_calls
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
        if expected_lock_inode != self.lock_inode or self.path_lock_inode != self.lock_inode:
            raise StopClosed("substituted lock")
        if self.record["lock"] != {"device": 1, "inode": expected_lock_inode}:
            raise StopClosed("persisted lock identity changed")
        mutate(self.record)
        self.record["previous_sha256"] = old_digest
        self.record["generation"] = generation + 1
        self.record["self_inode"] = int(self.record["self_inode"]) + 1

    def replace_lock_path(self, inode: int) -> None:
        self.path_lock_inode = inode

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

    def _plan(self, slot: str, node: str | None = None, prior: str | None = None) -> None:
        current = self.record[slot]
        if current["phase"] != "none" and not (
            slot == "comment" and current["phase"] == "bound"
        ):
            raise StopClosed("slot already owns an intent")
        self.persist(lambda r: r[slot].update(phase="planned", node=node, prior=prior))

    def plan_comment(self) -> None:
        self._plan("comment")

    def plan_create(self, proven_missing: bool = True) -> None:
        if not proven_missing:
            raise StopClosed("child is not proven missing")
        self._plan("create")

    def plan_link(self) -> None:
        create = self.record["create"]
        if create["phase"] != "bound" or create["node"] != "issue-node":
            raise StopClosed("link lacks the exact bound child")
        self._plan("link")

    def arm(self, slot: str) -> CallPermit:
        if self.record[slot]["phase"] != "planned":
            raise StopClosed("remote call lacks planned intent")
        self.persist(lambda r: r[slot].update(phase="in-flight"))
        return CallPermit(slot, int(self.record["generation"]))

    def execute(self, permit: CallPermit) -> None:
        if (
            permit.used or permit.generation != self.record["generation"]
            or self.record[permit.slot]["phase"] != "in-flight"
            or self.path_lock_inode != self.lock_inode
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

    def bind_create(self, candidates: list[dict[str, str]]) -> None:
        exact = [
            item for item in candidates
            if set(item) == {
                "node", "creator", "title", "body", "child_marker", "created_at"
            }
            and item["node"] == "issue-node" and item["creator"] == "actor"
            and item["title"] == "title" and item["body"] == "body"
            and item["child_marker"] == "program-child-marker"
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
        self._plan("comment", node=node, prior="old-body")

    def finish_patch(self, remote_body: str, result_node: str) -> CallPermit | None:
        if (
            self.record["comment"]["phase"] != "in-flight"
            or result_node != self.record["comment"]["node"]
            or self.record["next_graph"] is None
        ):
            raise StopClosed("PATCH result or prior body drifted")
        if remote_body == "old-body":
            return CallPermit("comment", int(self.record["generation"]))
        if remote_body != "intended-body":
            raise StopClosed("PATCH result or prior body drifted")
        self.persist(
            lambda record: (
                record["comment"].update(phase="bound", prior=None),
                record.update(
                    graph=copy.deepcopy(record["next_graph"]), next_graph=None
                ),
            )
        )
        return None

    def compose_leaf(self, position: str, generation: int, digest: str) -> None:
        snapshot = self.record["leaf_snapshots"].get(position)
        if snapshot != [generation, digest] or position not in self.record["graph"]:
            raise StopClosed("leaf ownership or evidence does not match graph")


class ProgramLedgerModelTests(unittest.TestCase):
    def test_fresh_and_persisted_resume_are_distinct(self) -> None:
        model = ProgramLedgerModel.fresh(False, False)
        with self.assertRaises(StopClosed):
            ProgramLedgerModel.fresh(True, False)
        resumed = ProgramLedgerModel.resume(
            model.record, model.lock_inode, model.record["self_inode"],
            model.digest(), model.record["authority"],
        )
        self.assertEqual(resumed.record, model.record)
        for args in (
            (None, 41, 101, None, model.record["authority"]),
            (model.record, None, 101, model.digest(), model.record["authority"]),
        ):
            with self.subTest(args=args):
                with self.assertRaises(StopClosed):
                    ProgramLedgerModel.resume(*args)

    def test_corrupt_lost_or_inode_substituted_ledger_stops(self) -> None:
        model = ProgramLedgerModel()
        corrupt = copy.deepcopy(model.record)
        corrupt["unknown"] = True
        bad_phase = copy.deepcopy(model.record)
        bad_phase["comment"]["phase"] = "garbage"
        bad_authority = copy.deepcopy(model.record)
        bad_authority["authority"] = ["other"] * 4
        bad_bound = copy.deepcopy(model.record)
        bad_bound["comment"]["phase"] = "bound"
        bad_next = copy.deepcopy(model.record)
        bad_next["next_graph"] = ["leaf-2"]
        bad_link = copy.deepcopy(model.record)
        bad_link["create"].update(phase="bound", node="issue-node")
        bad_link["link"].update(phase="planned", node="result", prior="unexpected")
        cases = (
            (corrupt, 41, 101, model.digest(), model.record["authority"]),
            (model.record, 41, 999, model.digest(), model.record["authority"]),
            (model.record, 41, 101, "0" * 64, model.record["authority"]),
            (model.record, 99, 101, model.digest(), model.record["authority"]),
            (bad_phase, 41, 101,
             hashlib.sha256(ProgramLedgerModel.canonical(bad_phase)).hexdigest(),
             model.record["authority"]),
            (bad_authority, 41, 101,
             hashlib.sha256(ProgramLedgerModel.canonical(bad_authority)).hexdigest(),
             model.record["authority"]),
            (bad_bound, 41, 101,
             hashlib.sha256(ProgramLedgerModel.canonical(bad_bound)).hexdigest(),
             model.record["authority"]),
            (bad_next, 41, 101,
             hashlib.sha256(ProgramLedgerModel.canonical(bad_next)).hexdigest(),
             model.record["authority"]),
            (bad_link, 41, 101,
             hashlib.sha256(ProgramLedgerModel.canonical(bad_link)).hexdigest(),
             model.record["authority"]),
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
            flags = os.O_CREAT | os.O_EXCL | os.O_RDWR | os.O_NOFOLLOW
            lock_fd = os.open(lock, flags, 0o600)
            self.addCleanup(os.close, lock_fd)
            with self.assertRaises(FileExistsError):
                os.open(lock, flags, 0o600)
            fcntl.flock(lock_fd, fcntl.LOCK_EX | fcntl.LOCK_NB)
            self.assertEqual(os.fstat(lock_fd).st_mode & 0o777, 0o600)
            competing_fd = os.open(lock, os.O_RDWR | os.O_NOFOLLOW)
            self.addCleanup(os.close, competing_fd)
            with self.assertRaises(BlockingIOError):
                fcntl.flock(competing_fd, fcntl.LOCK_EX | fcntl.LOCK_NB)
            lock_inode = os.fstat(lock_fd).st_ino
            ledger_fd = os.open(ledger, flags, 0o600)
            os.write(ledger_fd, b"old")
            os.fsync(ledger_fd)
            old_inode = os.fstat(ledger_fd).st_ino
            os.close(ledger_fd)
            temporary = root / "ledger.tmp"
            temporary_fd = os.open(temporary, flags, 0o600)
            os.write(temporary_fd, b"new")
            os.fsync(temporary_fd)
            new_inode = os.fstat(temporary_fd).st_ino
            os.close(temporary_fd)
            self.assertNotEqual(old_inode, new_inode)
            with self.assertRaises(FileExistsError):
                os.open(temporary, flags, 0o600)
            os.replace(temporary, ledger)
            directory_fd = os.open(root, os.O_RDONLY | os.O_DIRECTORY)
            os.fsync(directory_fd)
            os.close(directory_fd)
            self.assertEqual(os.stat(lock).st_ino, lock_inode)
            self.assertEqual(os.stat(ledger).st_ino, new_inode)
            replacement = root / "replacement.lock"
            replacement_fd = os.open(replacement, flags, 0o600)
            os.close(replacement_fd)
            os.replace(replacement, lock)
            self.assertNotEqual(os.stat(lock).st_ino, lock_inode)
            model = ProgramLedgerModel()
            model.replace_lock_path(os.stat(lock).st_ino)
            with self.assertRaises(StopClosed):
                model.persist(lambda record: None)
            model = ProgramLedgerModel()
            model.plan_comment()
            permit = model.arm("comment")
            model.replace_lock_path(os.stat(lock).st_ino)
            with self.assertRaises(StopClosed):
                model.execute(permit)

    def test_coordinate_digest_is_injective_for_hyphen_name_collision(self) -> None:
        first = ProgramLedgerModel.coordinate("R_foo-bar_baz", "I_1")
        second = ProgramLedgerModel.coordinate("R_foo_bar-baz", "I_2")
        self.assertNotEqual(first, second)

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
        model.plan_comment()
        model.arm("comment")
        resumed = ProgramLedgerModel.resume(
            model.record, 41, model.record["self_inode"], model.digest(),
            model.record["authority"],
        )
        with self.assertRaises(StopClosed):
            resumed.arm("comment")
        self.assertEqual(resumed.remote_calls["comment"], 0)

    def test_crash_after_each_remote_call_never_reposts(self) -> None:
        for slot in ("comment", "create", "link"):
            with self.subTest(slot=slot):
                model = ProgramLedgerModel()
                if slot == "comment":
                    model.plan_comment()
                elif slot == "create":
                    model.plan_create()
                else:
                    model.record["create"].update(phase="bound", node="issue-node")
                    model.plan_link()
                permit = model.arm(slot)
                model.execute(permit)
                with self.assertRaises(StopClosed):
                    model.execute(permit)
                resumed = ProgramLedgerModel.resume(
                    model.record, 41, model.record["self_inode"], model.digest(),
                    model.record["authority"], remote_calls=model.remote_calls,
                )
                with self.assertRaises(StopClosed):
                    resumed.arm(slot)
                self.assertEqual(model.remote_calls[slot], 1)

    def test_invisible_post_can_bind_later_from_persisted_intent(self) -> None:
        for report_present in (False, True):
            with self.subTest(report_present=report_present):
                model = ProgramLedgerModel()
                model.plan_comment()
                permit = model.arm("comment")
                model.execute(permit)
                resumed = ProgramLedgerModel.resume(
                    model.record, 41, model.record["self_inode"], model.digest(),
                    model.record["authority"], report_present=report_present,
                    remote_calls=model.remote_calls,
                )
                self.assertEqual(resumed.report_present, report_present)
                with self.assertRaises(StopClosed):
                    resumed.arm("comment")
                resumed.bind("comment", ["comment-node"])
                self.assertEqual(resumed.record["comment"]["phase"], "bound")
                self.assertEqual(resumed.remote_calls["comment"], 1)

    def test_invisible_child_create_and_link_bind_after_restart(self) -> None:
        model = ProgramLedgerModel()
        model.plan_create()
        create = model.arm("create")
        model.execute(create)
        resumed = ProgramLedgerModel.resume(
            model.record, 41, model.record["self_inode"], model.digest(),
            model.record["authority"], remote_calls=model.remote_calls,
        )
        resumed.bind("create", ["issue-node"])
        resumed.plan_link()
        link = resumed.arm("link")
        resumed.execute(link)
        again = ProgramLedgerModel.resume(
            resumed.record, 41, resumed.record["self_inode"], resumed.digest(),
            resumed.record["authority"], remote_calls=resumed.remote_calls,
        )
        again.bind_link("master", ["issue-node"], "parent-stream")
        self.assertEqual(again.remote_calls, {"comment": 0, "create": 1, "link": 1})

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
        model.plan_comment()
        permit = model.arm("comment")
        model.execute(permit)
        model.bind("comment", ["comment-node"])
        model.rekey(model.record["authority"], ["leaf-1", "leaf-2"], "comment-node")
        patch = model.arm("comment")
        model.execute(patch)
        model = ProgramLedgerModel.resume(
            model.record, 41, model.record["self_inode"], model.digest(),
            model.record["authority"], remote_calls=model.remote_calls,
        )
        with self.assertRaises(StopClosed):
            model.finish_patch("drifted-body", "comment-node")
        retry = model.finish_patch("old-body", "comment-node")
        self.assertIsNotNone(retry)
        self.assertEqual(model.record["graph"], ["leaf-1"])
        self.assertEqual(model.record["next_graph"], ["leaf-1", "leaf-2"])
        model.execute(retry)
        model = ProgramLedgerModel.resume(
            model.record, 41, model.record["self_inode"], model.digest(),
            model.record["authority"], remote_calls=model.remote_calls,
        )
        model.finish_patch("intended-body", "comment-node")
        self.assertEqual(model.record["comment"]["node"], "comment-node")
        self.assertEqual(model.record["graph"], ["leaf-1", "leaf-2"])
        self.assertIsNone(model.record["next_graph"])
        with self.assertRaises(StopClosed):
            model.rekey(["other"] * 4, ["bad"], "comment-node")

    def test_create_result_requires_exact_actor_bytes_and_child_marker(self) -> None:
        model = ProgramLedgerModel()
        model.plan_create()
        permit = model.arm("create")
        model.execute(permit)
        wrong = {"node": "issue-node", "creator": "other", "title": "title",
                 "body": "body", "child_marker": "program-child-marker",
                 "created_at": "2026-08-14T00:00:00.123Z"}
        with self.assertRaises(StopClosed):
            model.bind_create([wrong])
        exact = dict(wrong, creator="actor")
        model.bind_create([exact])

    def test_native_link_binds_parent_child_pair_without_edge_node(self) -> None:
        model = ProgramLedgerModel()
        with self.assertRaises(StopClosed):
            model.plan_link()
        model.record["create"].update(phase="bound", node="issue-node")
        model.plan_link()
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
        model.plan_create()
        permit = model.arm("create")
        model.execute(permit)
        for results in ([], ["one", "two"]):
            with self.subTest(results=results), self.assertRaises(StopClosed):
                model.bind("create", results)
        self.assertEqual(model.remote_calls["create"], 1)

    def test_create_and_link_cannot_plan_twice_or_out_of_order(self) -> None:
        model = ProgramLedgerModel()
        with self.assertRaises(StopClosed):
            model.plan_create(False)
        with self.assertRaises(StopClosed):
            model.plan_link()
        model.plan_create()
        create_permit = model.arm("create")
        model.execute(create_permit)
        model.bind("create", ["issue-node"])
        with self.assertRaises(StopClosed):
            model.plan_create()
        model.plan_link()
        link_permit = model.arm("link")
        model.execute(link_permit)
        model.bind_link("master", ["issue-node"], "digest")
        with self.assertRaises(StopClosed):
            model.plan_link()
        self.assertEqual(model.remote_calls, {"comment": 0, "create": 1, "link": 1})

    def test_leaf_composition_rejects_overlap_reorder_and_drift(self) -> None:
        model = ProgramLedgerModel()
        model.compose_leaf("leaf-1", 1, "a" * 64)
        for args in (("leaf-2", 1, "a" * 64), ("leaf-1", 2, "a" * 64),
                     ("leaf-1", 1, "b" * 64)):
            with self.subTest(args=args), self.assertRaises(StopClosed):
                model.compose_leaf(*args)


if __name__ == "__main__":
    unittest.main()
