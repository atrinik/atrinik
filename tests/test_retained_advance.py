from __future__ import annotations

import copy
from pathlib import Path
import unittest

from atrinik_workspace import retained_advance as advance
from atrinik_workspace.retained_advance import WorkspaceError


class RetainedAdvanceSchemaTests(unittest.TestCase):
    def setUp(self):
        self.raw = (Path(__file__).parents[1] / "atrinik_workspace/linux_portable.py").read_bytes()
        self.pins = advance.accepted_portable_pins(self.raw)

    def envelope(self):
        return {"schema_version": 1, "correction_sha256": "a" * 64,
                "declaration": {"accepted_wrapper_commit": "b" * 40,
                    "producer_blob_sha256": "c" * 64, "portable_pins": self.pins,
                    "classic_root": "/workspaces/owner/classic", "old_classic_head": "d" * 40,
                    "new_classic_head": self.pins["CONSUMER_COMMIT"], "profile": "owned-profile",
                    "scenario": "owned-player", "topology": "owned-server", "build_slot": "advanced-build"},
                "steps": [{"stage": "declare", "generation": 10, "predecessor_sha256": "e" * 64,
                           "request": {}, "observations": {}, "previous_topology_current": None}]}

    def test_accepted_pins_are_literals_and_exact_inventory(self):
        self.assertEqual(self.pins["CONSUMER_COMMIT"], "d926f6fd0418fb1af9060158c43d8d3ff5252580")
        for raw in (self.raw + b'\nIMAGE = "duplicate"\n', self.raw.replace(b'IMAGE = "', b'IMAGE = str("', 1),
                    self.raw.replace(b'@sha256:', b':latest#', 1), b'IMAGE = __import__("os").system("false")'):
            with self.subTest(raw=raw[-40:]), self.assertRaises(WorkspaceError):
                advance.accepted_portable_pins(raw)
        bad = copy.deepcopy(self.pins)
        bad["METADATA_HASHES"]["unrelated.json"] = "f" * 64
        with self.assertRaises(WorkspaceError):
            advance.validate_pins(bad)

    def test_append_preserves_prior_records_and_declaration(self):
        before = self.envelope()
        advance.require_append(None, before)
        after = copy.deepcopy(before)
        after["steps"].append({"stage": "plan", "generation": 12, "predecessor_sha256": "f" * 64,
                              "request": {"plan": "retained"}, "observations": {}, "previous_topology_current": None})
        advance.require_append(before, after)
        for change in ("root", "history", "order", "generation", "skip"):
            bad = copy.deepcopy(after)
            if change == "root": bad["declaration"]["classic_root"] = "/foreign/classic"
            elif change == "history": bad["steps"][0]["request"]["injected"] = True
            elif change == "order": bad["steps"].reverse()
            elif change == "generation": bad["steps"][1]["generation"] = 10
            else: bad["steps"][1]["stage"] = "topology"
            with self.subTest(change=change), self.assertRaises(WorkspaceError):
                advance.require_append(before, bad)

    def test_declaration_refuses_head_or_root_substitution(self):
        for field, value in (("new_classic_head", "f" * 40), ("classic_root", "/workspaces/owner/../foreign"),
                             ("classic_root", "relative"), ("build_slot", "../slot")):
            bad = self.envelope()
            bad["declaration"][field] = value
            with self.subTest(field=field), self.assertRaises(WorkspaceError):
                advance.validate_envelope(bad)
        bad = self.envelope()
        bad["declaration"]["old_classic_head"] = bad["declaration"]["new_classic_head"]
        with self.assertRaises(WorkspaceError):
            advance.validate_envelope(bad)

    def plan(self):
        value = {"schema_version": 1, "target": "server", "tests": True, "force_reconfigure": False,
                 "use_ccache": True, "targets": ["server"], "profile": {"name": "owned-profile", "stack": "classic"},
                 "manifest": {"schema_version": 1}, "checkout_states": {"classic": {
                     "path": "/workspaces/owner/classic", "head": self.pins["CONSUMER_COMMIT"], "dirty": False}},
                 "source_fingerprints": {"server": {}}, "git_observations": {"classic": {}},
                 "sources": {"server": "/workspaces/owner/classic/server"},
                 "execution_sources": {"server": "/workspaces/owner/workspace/build/source-generations/sealed/source/server"},
                 "wrapper_root": "/workspaces/owner", "workspace_root": "/workspaces/owner/workspace",
                 "builds_root": "/workspaces/owner/workspace/build", "build_key": "1" * 12,
                 "build_root": "/workspaces/owner/workspace/build/profiles/owned-profile-" + "1" * 12}
        value["plan_sha256"] = advance.digest(value)
        return value

    def check_plan(self, plan):
        return advance.validate_plan(plan, profile="owned-profile", wrapper="/workspaces/owner",
                                     workspace="/workspaces/owner/workspace", classic_root="/workspaces/owner/classic",
                                     classic_head=self.pins["CONSUMER_COMMIT"])

    def test_plan_binds_complete_digest_and_server_coordinates(self):
        plan = self.plan()
        self.assertEqual(self.check_plan(plan), plan)
        for field, value in (("target", "client"), ("tests", False), ("build_root", "/foreign/build"),
                             ("wrapper_root", "/foreign"), ("workspace_root", "/primary/workspace")):
            bad = copy.deepcopy(plan)
            bad[field] = value
            bad["plan_sha256"] = advance.digest({key: val for key, val in bad.items() if key != "plan_sha256"})
            with self.subTest(field=field), self.assertRaises(WorkspaceError):
                self.check_plan(bad)
        bad = copy.deepcopy(plan)
        bad["use_ccache"] = False
        with self.assertRaises(WorkspaceError):
            self.check_plan(bad)


if __name__ == "__main__":
    unittest.main()
