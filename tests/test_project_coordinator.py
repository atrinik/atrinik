"""Behavioral tests for the production coordinator, store and tracking executor."""
from __future__ import annotations

import copy
import json
import multiprocessing
import os
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch

from atrinik_workspace.project_coordinator import (
    ProjectError, attest, conflict, digest, new_project, record_worker, replan, require,
    reopen, reserve, retry, schedule, terminal_gaps, validate_plan, validate_project, worker_result,
)
from atrinik_workspace.project_coordinator_store import Store, decode, read_input
from atrinik_workspace.project_coordinator_github import (
    GitHub, apply_operation, cancel_operation, prepare_operation, refresh, relationship_guard,
)
from atrinik_workspace import project_coordinator_projects as projects
from atrinik_workspace import project_delivery as cli


def node(number, *, dependencies=None, writes=None, external=False, mode="issue", heavy=False):
    return {"id": f"atrinik/atrinik#{number}", "entry_mode": mode,
            "dependencies": dependencies or [], "reads": [], "writes": writes or [f"file-{number}"],
            "resources": [], "heavy": heavy, "external": external}


def plan(nodes=None):
    nodes = nodes or [node(2), node(3)]
    return {"parent": "atrinik/atrinik#1", "repositories": ["atrinik/atrinik"],
            "nodes": nodes, "acceptance": [{"id": "integrated", "description": "real integration",
                                           "owners": [n["id"] for n in nodes]}],
            "permissions": ["assign", "comment", "link", "dependency", "create-child", "close-parent"]}


def project(nodes=None):
    return new_project(plan(nodes), "zoeyrose", "test:explicit-project")


def race(root, expected, queue):
    try:
        Store(Path(root)).update(expected, lambda p: p["nodes"]["atrinik/atrinik#2"].update(detail="writer"))
        queue.put("won")
    except (ProjectError, OSError):
        queue.put("lost")


class SchedulerTests(unittest.TestCase):
    def test_replan_cannot_drop_blocked_attempt_reservation(self):
        p = project()
        request = reserve(p, 1, 1)[0]
        record_worker(p, request["coordinate"], request["attempt"], "agent-1")
        worker_result(p, request["coordinate"], request["attempt"], "blocked", "needs follow-up")
        changed = copy.deepcopy(p["plan"])
        changed["nodes"][0]["writes"] = ["different"]
        before = copy.deepcopy(p)
        with self.assertRaises(ProjectError):
            replan(p, changed)
        self.assertEqual(p, before)
        self.assertEqual(schedule(p, 1, 1)["ready"], [])

    def test_terminal_refresh_keeps_blocked_worker_until_recovered_result(self):
        p = project([node(2), node(3, dependencies=[{"id": "atrinik/atrinik#2", "condition": "merged"}])])
        gh = FakeGitHub()
        for observation in gh.observations.values():
            observation["terminal"] = False
        refresh(p, gh)
        request = reserve(p, 1, 1)[0]
        record_worker(p, request["coordinate"], request["attempt"], "agent-1")
        gh.observations[request["coordinate"]]["head"] = "changed"
        refresh(p, gh)
        gh.observations[request["coordinate"]]["terminal"] = True
        refresh(p, gh)
        self.assertEqual(p["nodes"][request["coordinate"]]["worker"], "agent-1")
        self.assertEqual(schedule(p, 1, 1)["ready"], [])
        resumed = reopen(p, request["coordinate"], request["attempt"], "same owner and merged head verified", 1)
        worker_result(p, request["coordinate"], resumed["attempt"], "ready", "fresh merged-head validation")
        refresh(p, gh)
        self.assertEqual(p["nodes"][request["coordinate"]]["state"], "merged")
        self.assertEqual(schedule(p, 1, 1)["ready"], ["atrinik/atrinik#3"])

    def test_completed_external_declaration_never_dispatches(self):
        p = project([node(2, external=True), node(3)])
        refresh(p, FakeGitHub())
        changed = copy.deepcopy(p["plan"])
        changed["nodes"][0]["writes"] = ["changed"]
        with self.assertRaises(ProjectError):
            replan(p, changed)
        p["nodes"]["atrinik/atrinik#2"]["state"] = "pending"
        self.assertNotIn("atrinik/atrinik#2", schedule(p, 16, 1)["ready"])

    def test_external_heavy_work_consumes_shared_budget(self):
        p = project([node(2, external=True, heavy=True), node(3, heavy=True)])
        self.assertEqual(schedule(p, 16, 1)["ready"], [])

    def test_parent_requirements_change_invalidates_attestation(self):
        p = project()
        gh = FakeGitHub()
        refresh(p, gh)
        attest(p, "integrated", "verified current requirements")
        self.assertEqual(terminal_gaps(p), [])
        gh.observations[p["plan"]["parent"]]["body"] = "new requirements"
        refresh(p, gh)
        self.assertTrue(any("stale acceptance" in gap for gap in terminal_gaps(p)))

    def test_replan_invalidates_accepted_dependency_gates(self):
        p = project([node(2), node(3, dependencies=[{"id": "atrinik/atrinik#2", "condition": "accepted"}])])
        refresh(p, FakeGitHub())
        attest(p, "integrated", "current acceptance")
        changed = copy.deepcopy(p["plan"])
        changed["acceptance"][0]["description"] = "additional requirements"
        replan(p, changed)
        p["nodes"]["atrinik/atrinik#3"]["state"] = "pending"
        self.assertEqual(schedule(p, 16, 1)["ready"], [])

    def test_head_drift_rejects_queued_result_and_keeps_reservation(self):
        p = project()
        gh = FakeGitHub()
        for observation in gh.observations.values():
            observation["terminal"] = False
        refresh(p, gh)
        request = reserve(p, 1, 1)[0]
        record_worker(p, request["coordinate"], request["attempt"], "agent-1")
        gh.observations[request["coordinate"]]["head"] = "new"
        refresh(p, gh)
        with self.assertRaises(ProjectError):
            worker_result(p, request["coordinate"], request["attempt"], "ready", "stale head proof")
        self.assertEqual(schedule(p, 1, 1)["ready"], [])

    def test_stopped_ready_worker_can_retry_exact_delivery(self):
        p = project()
        request = reserve(p, 1, 1)[0]
        record_worker(p, request["coordinate"], request["attempt"], "agent-1")
        worker_result(p, request["coordinate"], request["attempt"], "ready", "ready checkpoint")
        retry(p, request["coordinate"], request["attempt"], "runtime proves stopped; exact leaf handoff inspected")
        self.assertIsNone(p["nodes"][request["coordinate"]]["worker"])
        self.assertEqual(schedule(p, 1, 1)["ready"], [request["coordinate"]])

    def test_created_child_requires_graph_adoption_and_native_link(self):
        p = project()
        refresh(p, FakeGitHub())
        attest(p, "integrated", "current requirements")
        p["operations"]["test"] = {"kind": "create-child", "phase": "bound", "result": {"match": "I_new"}}
        self.assertIn("created child needs tracked graph and verified native parent link", terminal_gaps(p))

    def test_unbound_reservations_add_to_observed_workers(self):
        p = project()
        reserve(p, 4, 1, 2)
        revised = copy.deepcopy(p["plan"])
        revised["nodes"].append(node(4))
        replan(p, revised)
        self.assertEqual(schedule(p, 4, 1, 2)["ready"], [])

    def test_reopen_same_worker_for_findings(self):
        p = project()
        request = reserve(p, 1, 1)[0]
        record_worker(p, request["coordinate"], request["attempt"], "agent-1")
        worker_result(p, request["coordinate"], request["attempt"], "ready", "head checks complete")
        renewed = reopen(p, request["coordinate"], request["attempt"], "live owner; new reviewer finding", 1)
        with self.assertRaises(ProjectError):
            worker_result(p, request["coordinate"], request["attempt"], "ready", "queued old result")
        worker_result(p, request["coordinate"], renewed["attempt"], "ready", "fresh final diff passes")
        self.assertEqual(p["nodes"][request["coordinate"]]["worker"], "agent-1")

    def test_postmerge_revalidation_clears_blocked_terminal_owner(self):
        p = project([node(2), node(3, dependencies=[{"id": "atrinik/atrinik#2", "condition": "merged"}])])
        gh = FakeGitHub()
        refresh(p, gh)
        attest(p, "integrated", "first exact-head test")
        gh.observations["atrinik/atrinik#2"]["head"] = "changed"
        refresh(p, gh)
        self.assertEqual(p["nodes"]["atrinik/atrinik#3"]["state"], "blocked")
        attest(p, "integrated", "revalidated current integration")
        self.assertEqual(terminal_gaps(p), [])
    def test_disjoint_same_base_parallel(self):
        self.assertEqual(len(schedule(project(), 16, 1)["ready"]), 2)

    def test_diamond_dependencies(self):
        nodes = [node(2), node(3, dependencies=[{"id": "atrinik/atrinik#2", "condition": "merged"}]),
                 node(4, dependencies=[{"id": "atrinik/atrinik#2", "condition": "merged"}]),
                 node(5, dependencies=[{"id": f"atrinik/atrinik#{i}", "condition": "accepted"} for i in (3, 4)])]
        p = project(nodes)
        p["nodes"][nodes[0]["id"]]["state"] = "ready"
        self.assertEqual(schedule(p, 16, 1)["ready"], [])
        p["nodes"][nodes[0]["id"]]["state"] = "merged"
        self.assertEqual(schedule(p, 16, 1)["ready"], [nodes[1]["id"], nodes[2]["id"]])

    def test_cycles_missing_duplicates(self):
        for nodes in ([node(2), node(2)],
                      [node(2, dependencies=[{"id": "atrinik/atrinik#2", "condition": "merged"}])],
                      [node(2, dependencies=[{"id": "atrinik/atrinik#9", "condition": "merged"}])]):
            with self.subTest(nodes=nodes), self.assertRaises(ProjectError):
                project(nodes)

    def test_path_and_resource_conflicts(self):
        a, b = node(2, writes=["src"]), node(3, writes=["src/main.py"])
        self.assertTrue(conflict(a, b))
        b["writes"] = ["src-other"]
        self.assertFalse(conflict(a, b))
        b["reads"] = ["src/input"]
        self.assertTrue(conflict(a, b))
        for value in ("../x", "/root", "a/../b", "a//b", "x\\y"):
            with self.subTest(value=value), self.assertRaises(ProjectError):
                project([node(2, writes=[value])])

    def test_external_owner_blocks_overlapping_dispatch(self):
        p = project([node(2, writes=["."], external=True), node(3)])
        self.assertEqual(schedule(p, 16, 1)["ready"], [])
        self.assertEqual(p["nodes"]["atrinik/atrinik#2"]["state"], "external")

    def test_capacity_counts_open_reviewers_and_heavy_jobs(self):
        p = project([node(2, heavy=True), node(3, heavy=True), node(4)])
        self.assertEqual(len(schedule(p, 16, 1)["ready"]), 2)
        self.assertEqual(schedule(p, 16, 1, open_workers=16)["ready"], [])
        self.assertEqual(len(schedule(p, 2, 1, open_workers=1)["ready"]), 1)

    def test_reservation_worker_identity_and_stale_results(self):
        p = project()
        requests = reserve(p, 1, 1)
        r = requests[0]
        self.assertEqual(reserve(p, 1, 1), [])
        record_worker(p, r["coordinate"], r["attempt"], "agent-1")
        with self.assertRaises(ProjectError):
            record_worker(p, r["coordinate"], r["attempt"], "agent-2")
        with self.assertRaises(ProjectError):
            worker_result(p, r["coordinate"], "stale", "ready", "evidence")
        worker_result(p, r["coordinate"], r["attempt"], "blocked", "auth unavailable")
        retry(p, r["coordinate"], r["attempt"], "worker closed; unchanged leaf inspected")
        self.assertEqual(p["nodes"][r["coordinate"]]["state"], "pending")

    def test_explicit_pr_mode_and_no_implicit_merge(self):
        p = project([node(2, mode="PR")])
        self.assertEqual(reserve(p, 1, 1)[0]["entry_mode"], "PR")
        changed = plan()
        changed["permissions"].append("merge")
        with self.assertRaises(ProjectError):
            validate_plan(changed)

    def test_replan_preserves_authority_and_active_work(self):
        p = project()
        newer = plan([node(2), node(3), node(4)])
        replan(p, newer)
        self.assertIn("atrinik/atrinik#4", p["nodes"])
        reserve(p, 1, 1)
        newer["nodes"][0]["writes"] = ["new"]
        with self.assertRaises(ProjectError):
            replan(p, newer)
        newer = copy.deepcopy(p["plan"])
        newer["repositories"].append("atrinik/classic")
        with self.assertRaises(ProjectError):
            replan(p, newer)

    def test_acceptance_is_not_child_count(self):
        p = project()
        p["observations"][p["plan"]["parent"]] = {"complete": True, "work_resolved": True}
        for ident, state in p["nodes"].items():
            state["state"] = "merged"
            p["observations"][ident] = {"complete": True, "terminal": True, "head": "a"}
        self.assertTrue(terminal_gaps(p))
        attest(p, "integrated", "exact final integration test evidence")
        self.assertEqual(terminal_gaps(p), [])
        p["observations"]["atrinik/atrinik#2"]["head"] = "b"
        self.assertTrue(terminal_gaps(p))

    def test_actor_and_schema_validation(self):
        with self.assertRaises(ProjectError):
            new_project(plan(), "", "authority")
        p = project()
        p["schema_version"] = True
        with self.assertRaises(ProjectError):
            validate_project(p)


@unittest.skipUnless(os.name == "posix", "canonical Linux filesystem contract")
class StoreTests(unittest.TestCase):
    def test_near_limit_inspect_snapshot_round_trips(self):
        from atrinik_workspace.project_coordinator_store import LIMIT
        p = project([node(i) for i in range(2, 258)])
        for state in p["nodes"].values():
            state["detail"] = "x" * 7760
        snapshot = self.store.create(p)
        raw = json.dumps(snapshot, indent=2).encode()
        self.assertGreater(len(raw), LIMIT)
        expected = self.store.root / "expected.json"
        expected.write_bytes(raw)
        parsed = read_input(expected, 2 * LIMIT)
        updated, _ = self.store.update(parsed, lambda d: None)
        self.assertEqual(updated["generation"], 2)
    def oversized_pretty_document(self):
        p = project([node(i) for i in range(2, 258)])
        for state in p["nodes"].values():
            state["detail"] = "x" * 7780
        from atrinik_workspace.project_coordinator import canonical
        self.assertLess(len(canonical(p)), 2 * 1024 * 1024)
        self.assertGreater(len(json.dumps(p, indent=2, sort_keys=True).encode()), 2 * 1024 * 1024)
        return p

    def test_create_checks_exact_persisted_size_before_any_write(self):
        with self.assertRaisesRegex(ProjectError, "persisted byte"):
            self.store.create(self.oversized_pretty_document())
        self.assertEqual(list(self.store.root.iterdir()), [])

    def test_oversized_update_preserves_readable_predecessor(self):
        initial = self.store.create(project())
        oversized = self.oversized_pretty_document()
        with self.assertRaisesRegex(ProjectError, "persisted byte"):
            self.store.update(initial, lambda p: p.update(oversized))
        self.assertEqual(self.store.inspect(), initial)
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.root = Path(self.tmp.name)
        self.store = Store(self.root)

    def tearDown(self):
        self.tmp.cleanup()

    def test_roundtrip_and_stale_cas(self):
        initial = self.store.create(project())
        current, _ = self.store.update(initial, lambda p: p["nodes"]["atrinik/atrinik#2"].update(detail="note"))
        self.assertEqual(current["generation"], 2)
        with self.assertRaises(ProjectError):
            self.store.update(initial, lambda p: None)
        self.assertEqual(self.store.inspect(), current)

    def test_concurrent_cas_has_one_winner(self):
        initial = self.store.create(project())
        ctx = multiprocessing.get_context("fork")
        queue = ctx.Queue()
        processes = [ctx.Process(target=race, args=(str(self.root), initial, queue)) for _ in range(2)]
        for process in processes:
            process.start()
        for process in processes:
            process.join(10)
            self.assertFalse(process.is_alive())
        self.assertCountEqual([queue.get(timeout=2), queue.get(timeout=2)], ["won", "lost"])

    def test_lost_initialization_never_reinitializes(self):
        (self.root / "project.lock").touch(mode=0o600)
        with self.assertRaises(FileExistsError):
            self.store.create(project())
        self.assertFalse((self.root / "project.json").exists())

    def test_symlink_and_permission_rejection(self):
        self.store.create(project())
        (self.root / "project.json").chmod(0o666)
        with self.assertRaises(ProjectError):
            self.store.inspect()
        other = self.root / "link"
        other.symlink_to(self.root, target_is_directory=True)
        with self.assertRaises(OSError):
            Store(other).inspect()

    def test_replaced_lock_prevents_commit(self):
        initial = self.store.create(project())
        def replace(p):
            (self.root / "project.lock").rename(self.root / "old.lock")
            (self.root / "project.lock").touch(mode=0o600)
        with self.assertRaises(ProjectError):
            self.store.update(initial, replace)
        self.assertEqual(self.store.inspect()["generation"], 1)

    def test_invalid_change_does_not_write(self):
        initial = self.store.create(project())
        with self.assertRaises(ProjectError):
            self.store.update(initial, lambda p: p.update(actor="other"))
        self.assertEqual(self.store.inspect(), initial)

    def test_bounded_duplicate_json(self):
        for raw in (b'{"a":1,"a":2}', b'{"a":NaN}', b" " * (2 * 1024 * 1024 + 1)):
            with self.assertRaises(ProjectError):
                decode(raw)


class FakeGitHub:
    def __init__(self):
        self.writes = []
        self.comments = []
        self.invisible = False
        self.fail_after_write = False
        self.login = "zoeyrose"
        self.issues = {f"atrinik/atrinik#{i}": {"id": i, "number": i, "node_id": f"I_{i}",
                       "state": "open", "state_reason": None, "assignees": [], "body": "human text"}
                       for i in (1, 2, 3)}
        self.observations = {key: {"node_id": value["node_id"], "complete": True,
                                  "terminal": key != "atrinik/atrinik#1", "work_resolved": True, "head": "a"}
                             for key, value in self.issues.items()}

    def actor(self):
        return self.login

    def issue(self, ident):
        return copy.deepcopy(self.issues[ident])

    def observe(self, ident, mode):
        return copy.deepcopy(self.observations[ident])

    def pages(self, path):
        return copy.deepcopy([] if self.invisible else self.comments) if path.endswith("/comments") else []

    def request(self, path, method="GET", payload=None):
        self.writes.append((path, method, payload))
        if path.endswith("/comments"):
            self.comments.append({"id": 100, "node_id": "C_100", "body": payload["body"],
                                  "user": {"login": self.login}})
        elif payload == {"state": "closed", "state_reason": "completed"}:
            self.issues["atrinik/atrinik#1"].update(payload)
        if self.fail_after_write:
            raise ProjectError("response lost")
        return None


@unittest.skipUnless(os.name == "posix", "canonical Linux filesystem contract")
class TrackingTests(unittest.TestCase):
    def test_independent_same_title_child_does_not_strand_unstarted_intent(self):
        snapshot, op = self.store.update(self.initial, lambda p:
            prepare_operation(p, self.gh, "create-child", "atrinik/atrinik#1",
                              {"repository": "atrinik/atrinik", "title": "New child", "body": "needed"}))
        unrelated = [{"id": 7, "node_id": "I_new", "title": "New child", "body": "human-authored"}]
        with patch.object(self.gh, "pages", return_value=unrelated):
            with self.assertRaises(ProjectError):
                apply_operation(self.store, snapshot, op["id"], self.gh)
            retired = cancel_operation(self.store, snapshot, op["id"], self.gh)
        self.assertEqual(retired["document"]["operations"][op["id"]]["phase"], "cancelled")
        self.assertEqual(self.gh.writes, [])
    def test_independently_satisfied_assignment_can_retire_unstarted(self):
        snapshot, op = self.store.update(self.initial, lambda p:
            prepare_operation(p, self.gh, "assign", "atrinik/atrinik#2", {"login": "zoeyrose"}))
        self.gh.issues["atrinik/atrinik#2"]["assignees"] = [{"login": "zoeyrose"}]
        result = cancel_operation(self.store, snapshot, op["id"], self.gh)
        self.assertEqual(result["document"]["operations"][op["id"]]["phase"], "cancelled")
        self.assertEqual(self.gh.writes, [])

    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.store = Store(Path(self.tmp.name))
        self.gh = FakeGitHub()
        self.initial = self.store.create(project())

    def tearDown(self):
        self.tmp.cleanup()

    def planned(self):
        snapshot, op = self.store.update(self.initial, lambda p:
            prepare_operation(p, self.gh, "comment", "atrinik/atrinik#1", {"body": "progress"}))
        return snapshot, op["id"]

    def test_successful_comment_preserves_human_body(self):
        expected, ident = self.planned()
        result = apply_operation(self.store, expected, ident, self.gh)
        self.assertEqual(result["document"]["operations"][ident]["phase"], "bound")
        self.assertEqual(len(self.gh.writes), 1)
        self.assertEqual(self.gh.issues["atrinik/atrinik#1"]["body"], "human text")
        apply_operation(self.store, result, ident, self.gh)
        self.assertEqual(len(self.gh.writes), 1)

    def test_accepted_but_lost_response_reconciles_without_repost(self):
        expected, ident = self.planned()
        self.gh.fail_after_write = True
        with self.assertRaises(ProjectError):
            apply_operation(self.store, expected, ident, self.gh)
        self.gh.fail_after_write = False
        result = apply_operation(self.store, self.store.inspect(), ident, self.gh, reconcile=True)
        self.assertEqual(result["document"]["operations"][ident]["phase"], "bound")
        self.assertEqual(len(self.gh.writes), 1)

    def test_invisible_result_never_reposts(self):
        expected, ident = self.planned()
        self.gh.invisible = True
        with self.assertRaises(ProjectError):
            apply_operation(self.store, expected, ident, self.gh)
        for _ in range(2):
            with self.assertRaises(ProjectError):
                apply_operation(self.store, self.store.inspect(), ident, self.gh)
        self.assertEqual(len(self.gh.writes), 1)

    def test_pre_call_drift_and_wrong_actor_make_no_write(self):
        expected, ident = self.planned()
        self.gh.login = "foreign"
        with self.assertRaises(ProjectError):
            apply_operation(self.store, expected, ident, self.gh)
        self.assertEqual(self.gh.writes, [])

    def test_stale_cas_no_remote_mutation(self):
        expected, ident = self.planned()
        with self.assertRaises(ProjectError):
            apply_operation(self.store, self.initial, ident, self.gh)
        self.assertEqual(self.gh.writes, [])

    def test_unstarted_intent_can_cancel_after_unrelated_comment(self):
        expected, ident = self.planned()
        self.gh.comments.append({"id": 7, "node_id": "C_human", "body": "human update"})
        cancelled = cancel_operation(self.store, expected, ident, self.gh)
        with self.assertRaises(ProjectError):
            apply_operation(self.store, cancelled, ident, self.gh)
        self.assertEqual(self.gh.writes, [])
        _, op = self.store.update(cancelled, lambda p:
            prepare_operation(p, self.gh, "comment", "atrinik/atrinik#1", {"body": "updated progress"}))
        self.assertNotEqual(op["id"], ident)

    def test_uncertain_started_intent_cannot_cancel(self):
        expected, ident = self.planned()
        self.gh.invisible = True
        with self.assertRaises(ProjectError):
            apply_operation(self.store, expected, ident, self.gh)
        with self.assertRaises(ProjectError):
            cancel_operation(self.store, self.store.inspect(), ident, self.gh)
        self.assertEqual(len(self.gh.writes), 1)

    def test_external_issue_cannot_be_claimed(self):
        p = project([node(2, external=True)])
        with self.assertRaises(ProjectError):
            prepare_operation(p, self.gh, "assign", "atrinik/atrinik#2", {"login": "zoeyrose"})

    def test_parent_close_requires_live_acceptance(self):
        with self.assertRaises(ProjectError):
            prepare_operation(project(), self.gh, "close-parent", "atrinik/atrinik#1", {})
        p = project()
        refresh(p, self.gh)
        attest(p, "integrated", "verified exact-head integration evidence")
        store = Store(Path(self.tmp.name) / "accepted")
        store.root.mkdir(mode=0o700)
        snapshot = store.create(p)
        snapshot, op = store.update(snapshot, lambda d:
            prepare_operation(d, self.gh, "close-parent", "atrinik/atrinik#1", {}))
        result = apply_operation(store, snapshot, op["id"], self.gh)
        self.assertEqual(result["document"]["operations"][op["id"]]["phase"], "bound")

    def test_post_plan_acceptance_drift_blocks_close(self):
        p = project()
        refresh(p, self.gh)
        attest(p, "integrated", "verified acceptance")
        snapshot, op = self.store.update(self.initial, lambda d:
            (d.update(p), prepare_operation(d, self.gh, "close-parent", "atrinik/atrinik#1", {}))[1])
        self.gh.observations["atrinik/atrinik#2"]["head"] = "new"
        with self.assertRaises(ProjectError):
            apply_operation(self.store, snapshot, op["id"], self.gh)
        self.assertEqual(self.gh.writes, [])

    def test_refresh_partial_failure_does_not_replace_evidence(self):
        p = project()
        original = copy.deepcopy(p)
        with patch.object(self.gh, "observe", side_effect=[{"complete": True}, ProjectError("incomplete")]):
            with self.assertRaises(ProjectError):
                refresh(p, self.gh)
        self.assertEqual(p, original)

    def test_incomplete_pagination_stops(self):
        gh = GitHub()
        with patch.object(gh, "request", return_value=[{"id": i} for i in range(100)]):
            with self.assertRaises(ProjectError):
                gh.pages("repos/atrinik/atrinik/issues")

    def test_malformed_journal_route_rejected(self):
        expected, ident = self.planned()
        with self.assertRaises(ProjectError):
            self.store.update(expected, lambda p: p["operations"][ident].update(path="repos/atrinik/atrinik/merges"))

    def test_journal_payload_cannot_add_unapproved_mutation(self):
        expected, ident = self.planned()
        with self.assertRaises(ProjectError):
            self.store.update(expected, lambda p: p["operations"][ident]["request"].update(state="closed"))
        self.assertEqual(self.store.inspect(), expected)

    def test_graphql_errors_are_not_success(self):
        import subprocess
        result = subprocess.CompletedProcess([], 0, b'{"errors":[{"message":"denied"}]}', b"")
        with patch("subprocess.run", return_value=result), self.assertRaises(ProjectError):
            GitHub().request("graphql", "POST", {"query": "query { viewer { login } }"})

    def test_timeline_without_rest_ids_is_preserved(self):
        events = [{"event": "cross-referenced", "source": {"issue": {"number": i}}} for i in (2, 3)]
        with patch.object(GitHub, "request", return_value=events):
            self.assertEqual(GitHub().pages("repos/atrinik/atrinik/issues/1/timeline"), events)

    def test_multihop_dependency_cycle_rejected(self):
        op = {"kind": "dependency", "target": "atrinik/atrinik#1",
              "payload": {"issue": "atrinik/atrinik#2"}, "request": {"issue_id": 2}}
        with patch.object(self.gh, "pages", side_effect=[
            [{"html_url": "https://github.com/atrinik/atrinik/issues/3"}],
            [{"html_url": "https://github.com/atrinik/atrinik/issues/1"}],
        ]), self.assertRaisesRegex(ProjectError, "cycle"):
            relationship_guard(op, self.gh)


class ProjectStatusTests(unittest.TestCase):
    def setUp(self):
        self.payload = {"project": "P_1", "item": "ITEM_2", "field": "F_3", "option": "O_4", "status": "Review"}
        self.op = {"target": "atrinik/atrinik#2", "payload": self.payload}
        self.gh = FakeGitHub()
        self.data = {"project": {"id": "P_1", "title": "Atrinik work", "owner": {"login": "atrinik"}},
                     "item": {"id": "ITEM_2", "project": {"id": "P_1"}, "content": {"id": "I_2"},
                              "fieldValueByName": None},
                     "field": {"id": "F_3", "name": "Status", "project": {"id": "P_1"},
                               "options": [{"id": "O_4", "name": "Review"}]}}

    def test_existing_exact_item_and_field(self):
        with patch.object(self.gh, "request", return_value={"data": self.data}):
            self.assertIsNone(projects.observe(self.op, self.gh)["match"])
            self.data["item"]["fieldValueByName"] = {"optionId": "O_4", "field": {"id": "F_3"}}
            self.assertEqual(projects.observe(self.op, self.gh)["match"], "ITEM_2")

    def test_foreign_item_and_option_block(self):
        for field, value in (("content", {"id": "foreign"}), ("project", {"id": "foreign"})):
            with self.subTest(field=field):
                data = copy.deepcopy(self.data)
                data["item"][field] = value
                with patch.object(self.gh, "request", return_value={"data": data}), self.assertRaises(ProjectError):
                    projects.observe(self.op, self.gh)
        self.data["field"]["options"] = []
        with patch.object(self.gh, "request", return_value={"data": self.data}), self.assertRaises(ProjectError):
            projects.observe(self.op, self.gh)

    def test_done_requires_live_terminal_issue(self):
        self.payload["status"] = "Done"
        self.gh.observations["atrinik/atrinik#2"]["terminal"] = False
        with self.assertRaises(ProjectError):
            projects.gate(project(), self.op, self.gh)

    def test_only_fixed_project_mutation(self):
        self.assertEqual(projects.validate(self.payload)["query"], projects.MUTATION)
        self.payload["query"] = "mutation { mergePullRequest }"
        with self.assertRaises(ProjectError):
            projects.validate(self.payload)


@unittest.skipUnless(os.name == "posix", "canonical Linux filesystem contract")
class CLITests(unittest.TestCase):
    def test_parser_requires_real_capacity_and_expected_snapshot(self):
        from contextlib import redirect_stderr
        import io
        for argv in (["dispatch", "--capacity", "16"], ["plan", "--capacity", "16"]):
            with redirect_stderr(io.StringIO()), self.assertRaises(SystemExit):
                cli.parser().parse_args(argv)

    def test_unsafe_context_never_initializes(self):
        import subprocess
        result = subprocess.CompletedProcess([], 0, b'{"authoritative":false,"status":"unknown-or-unsafe"}', b"")
        with patch("subprocess.run", return_value=result), self.assertRaises(ProjectError):
            cli.context()

    def test_cli_initialization_and_dispatch_cas(self):
        with tempfile.TemporaryDirectory() as directory:
            wrapper = Path(directory)
            supplied = wrapper / "plan.json"
            supplied.write_text(json.dumps(plan()))
            with patch.object(cli, "context", return_value=wrapper), patch("subprocess.run") as run:
                run.return_value.returncode = 0
                initialized = cli.run(cli.parser().parse_args(["init", "--plan", str(supplied),
                                      "--authority", "test:session"]), FakeGitHub())
                expected = wrapper / "expected.json"
                expected.write_text(json.dumps(initialized["snapshot"]))
                root = initialized["root"]
                args = cli.parser().parse_args(["--root", root, "dispatch", "--capacity", "4",
                     "--open-workers", "2", "--expected", str(expected)])
                dispatched = cli.run(args, FakeGitHub())
                # Fake live issues were already terminal; do not duplicate completed work.
                self.assertEqual(dispatched["result"], [])
                with self.assertRaisesRegex(ProjectError, "stale"):
                    cli.run(args, FakeGitHub())

    def test_fifo_input_rejected_without_blocking(self):
        with tempfile.TemporaryDirectory() as directory:
            fifo = Path(directory) / "input"
            os.mkfifo(fifo)
            with self.assertRaises(ProjectError):
                read_input(fifo)


class CheckObservationTests(unittest.TestCase):
    def test_parent_own_open_pr_failed_checks_and_blockers_prevent_close(self):
        for scenario in ("open-pr", "failed-checks", "open-blocker"):
            with self.subTest(scenario=scenario):
                gh = GitHub()
                issue = {"id": 1, "node_id": "I_1", "number": 1, "state": "open", "body": "parent"}
                pr = {"head": {"sha": "a" * 40}, "base": {"sha": "b" * 40},
                      "state": "closed", "merged_at": None if scenario == "open-pr" else "date"}
                timeline = [{"source": {"issue": {"pull_request": {}, "html_url": "https://github.com/atrinik/atrinik/pull/9"}}}]
                timeline[0]["source"]["issue"]["pull_request"] = {"url": "present"}
                blocker = {"node_id": "I_9", "state": "open", "state_reason": None}
                with patch.object(gh, "request", side_effect=[issue, pr]), patch.object(gh, "pages", side_effect=[
                    timeline, [], [blocker] if scenario == "open-blocker" else []]), patch.object(gh, "checks",
                    return_value={"complete": True, "passing": scenario != "failed-checks"}):
                    observed = gh.observe("atrinik/atrinik#1", "issue")
                p = project()
                fake = FakeGitHub()
                fake.observations["atrinik/atrinik#1"] = observed
                refresh(p, fake)
                attest(p, "integrated", "all declared children accepted")
                self.assertTrue(any(gap.startswith("parent:") for gap in terminal_gaps(p)))
                with self.assertRaises(ProjectError):
                    prepare_operation(p, fake, "close-parent", "atrinik/atrinik#1", {})

    def test_failed_or_pending_checks_block_terminal(self):
        for state, conclusion in (("completed", "failure"), ("in_progress", None), ("completed", "cancelled")):
            with self.subTest(state=state, conclusion=conclusion):
                gh = GitHub()
                with patch.object(gh, "pages", side_effect=[
                    [{"id": 1, "status": state, "conclusion": conclusion}], []]):
                    self.assertFalse(gh.checks("atrinik/atrinik", "a" * 40)["passing"])

    def test_superseded_status_does_not_block_success(self):
        gh = GitHub()
        with patch.object(gh, "pages", side_effect=[[], [
            {"context": "ci", "state": "success"}, {"context": "ci", "state": "failure"}]]):
            self.assertTrue(gh.checks("atrinik/atrinik", "a" * 40)["passing"])

    def test_collection_total_must_match_complete_scan(self):
        gh = GitHub()
        with patch.object(gh, "request", return_value={"total_count": 2, "check_runs": [{"id": 1}]}):
            with self.assertRaises(ProjectError):
                gh.pages("repos/atrinik/atrinik/commits/sha/check-runs", "check_runs")


if __name__ == "__main__":
    unittest.main()
