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

    def assert_rejected(self, message, operation):
        with self.assertRaisesRegex(WorkspaceError, "^" + message + "$"):
            operation()

    def content_input(self):
        return {"repository": "atrinik/content", "repository_node_id": "R_content", "branch": "main",
                "root": "/workspaces/owner/content", "old_head": "1" * 40, "new_head": "2" * 40,
                "tree": "3" * 40, "pull_request": 265, "pull_request_node_id": "P_content",
                "merge_commit": "2" * 40}

    def test_content_only_and_append_to_existing_declaration_preserve_history(self):
        before = self.envelope()
        after = copy.deepcopy(before)
        after["content_input"] = self.content_input()
        after["steps"].append({"stage": "plan", "generation": 11, "predecessor_sha256": "f" * 64,
                               "request": {}, "observations": {}, "previous_topology_current": None})
        advance.require_append(before, after)
        bad = copy.deepcopy(after)
        bad["declaration"]["classic_root"] = "/changed/classic"
        with self.assertRaisesRegex(WorkspaceError, "historical declaration/evidence is immutable"):
            advance.require_append(before, bad)
        content_only = self.envelope()
        content_only["content_input"] = self.content_input()
        content_only["declaration"]["old_classic_head"] = self.pins["CONSUMER_COMMIT"]
        advance.require_append(None, content_only)
        later = copy.deepcopy(after)
        later["steps"].append({"stage": "built", "generation": 12, "predecessor_sha256": "f" * 64,
                               "request": {}, "observations": {}, "previous_topology_current": None})
        later["content_input"]["new_head"] = "4" * 40
        with self.assertRaisesRegex(WorkspaceError, "historical declaration/evidence is immutable"):
            advance.require_append(after, later)
        no_content = copy.deepcopy(after)
        del no_content["content_input"]
        with self.assertRaisesRegex(WorkspaceError, "content intent must precede build production"):
            advance.require_append(no_content, later)

    def test_completed_build_accepts_one_content_successor_only(self):
        before = self.envelope()
        for stage, generation in (("plan", 11), ("built", 12)):
            before["steps"].append({"stage": stage, "generation": generation, "predecessor_sha256": "f" * 64,
                                    "request": {}, "observations": {}, "previous_topology_current": None})
        after = copy.deepcopy(before)
        after["content_input"] = {**self.content_input(), "build_slot": "content-tested-build"}
        after["steps"].append({"stage": "content-plan", "generation": 13, "predecessor_sha256": "f" * 64,
                               "request": {}, "observations": {}, "previous_topology_current": None})
        advance.require_append(before, after)
        self.assertEqual(advance.build_slot(after), "content-tested-build")
        self.assertEqual(advance.build_slot(before), "advanced-build")
        for stage, generation in (("content-built", 14), ("topology", 15)):
            next_value = copy.deepcopy(after)
            next_value["steps"].append({"stage": stage, "generation": generation, "predecessor_sha256": "f" * 64,
                                       "request": {}, "observations": {},
                                       "previous_topology_current": {} if stage == "topology" else None})
            advance.require_append(after, next_value)
            after = next_value
        bad = copy.deepcopy(after)
        bad["steps"][4]["stage"] = "built"
        with self.assertRaisesRegex(WorkspaceError, "ordered prefix"):
            advance.validate_envelope(bad)
        for value in ("../foreign", None):
            bad = copy.deepcopy(after)
            bad["content_input"]["build_slot"] = value
            with self.assertRaisesRegex(WorkspaceError, "build slot is invalid"):
                advance.validate_envelope(bad)

    def test_content_schema_rejects_missing_foreign_or_mutable_provenance(self):
        content = self.content_input()
        for field, value in (("repository", "atrinik/foreign"), ("branch", "1.x"),
                             ("new_head", content["old_head"]), ("new_head", "main"),
                             ("root", "/owner/../content"), ("root", "/owner/content\n"),
                             ("pull_request", True), ("pull_request", 0),
                             ("repository_node_id", ""), ("merge_commit", None)):
            bad = {**content, field: value}
            with self.subTest(field=field, value=value), self.assertRaises(WorkspaceError):
                advance.validate_content_input(bad)
        for field in content:
            bad = dict(content)
            del bad[field]
            with self.subTest(missing=field), self.assertRaises(WorkspaceError):
                advance.validate_content_input(bad)

    def test_content_plan_requires_declared_read_only_source(self):
        content = self.content_input()
        plan = self.plan()
        plan["retained_content_input"] = content["new_head"]
        plan["checkout_states"]["content"] = {"path": content["root"], "head": content["new_head"], "dirty": False}
        plan["sources"]["content"] = content["root"]
        plan["execution_sources"]["content"] = content["root"]
        plan["source_fingerprints"]["content"] = {content["root"]: "a" * 64}
        plan["git_observations"]["content"] = {"clean": True}
        def check(value):
            value["plan_sha256"] = advance.digest({key: val for key, val in value.items() if key != "plan_sha256"})
            return advance.validate_plan(value, profile="owned-profile", wrapper="/workspaces/owner",
                                         workspace="/workspaces/owner/workspace", classic_root="/workspaces/owner/classic",
                                         classic_head=self.pins["CONSUMER_COMMIT"], content_input=content)
        self.assertEqual(check(plan), plan)
        for field in ("head", "path"):
            bad = copy.deepcopy(plan)
            bad["checkout_states"]["content"][field] = "foreign"
            with self.subTest(field=field), self.assertRaises(WorkspaceError):
                check(bad)
        bad = copy.deepcopy(plan)
        bad["execution_sources"]["content"] = "/foreign/content"
        with self.assertRaisesRegex(WorkspaceError, "read-only source provenance"):
            check(bad)

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

    def test_accepted_pins_reject_bounded_and_malformed_producer_declarations(self):
        cases = (
            (b"#" * (1024 * 1024 + 1),
             "retained advance producer declaration exceeds bound"),
            (b"\xff", "retained advance producer declaration is invalid"),
            (b"IMAGE =", "retained advance producer declaration is invalid"),
            (self.raw.replace(b"IMAGE = ", b"IMAGE = build_image() # ", 1),
             "retained advance producer pin is not a literal"),
            (self.raw.replace(b"PLATFORM_MANIFEST =", b"IGNORED_MANIFEST =", 1),
             "retained advance portable pins has an unsupported shape"),
        )
        for raw, message in cases:
            with self.subTest(message=message):
                self.assert_rejected(message, lambda raw=raw: advance.accepted_portable_pins(raw))

    def test_pins_reject_mutable_image_and_malformed_types_or_metadata(self):
        cases = (
            ("IMAGE", "ghcr.io/atrinik/classic-portable-build:latest",
             "retained advance portable image is not an immutable producer"),
            ("PLATFORM_MANIFEST", "sha256:" + "F" * 64,
             "retained advance platform manifest is invalid"),
            ("CONSUMER_COMMIT", 1, "retained advance CONSUMER_COMMIT is invalid"),
        )
        for field, value, message in cases:
            bad = copy.deepcopy(self.pins)
            bad[field] = value
            with self.subTest(field=field):
                self.assert_rejected(message, lambda bad=bad: advance.validate_pins(bad))
        bad = copy.deepcopy(self.pins)
        bad["METADATA_HASHES"]["contract.json"] = "not-a-checksum"
        self.assert_rejected("retained advance contract.json is invalid", lambda: advance.validate_pins(bad))

    def test_append_preserves_prior_records_and_declaration(self):
        before = self.envelope()
        advance.require_append(None, before)
        after = copy.deepcopy(before)
        after["steps"].append({"stage": "plan", "generation": 12, "predecessor_sha256": "f" * 64,
                              "request": {"plan": "retained"}, "observations": {}, "previous_topology_current": None})
        advance.require_append(before, after)
        messages = {
            "root": "retained advance historical declaration/evidence is immutable",
            "history": "retained advance historical declaration/evidence is immutable",
            "order": "retained advance steps are not an ordered prefix",
            "generation": "retained advance steps are not an ordered prefix",
            "skip": "retained advance steps are not an ordered prefix",
        }
        for change in ("root", "history", "order", "generation", "skip"):
            bad = copy.deepcopy(after)
            if change == "root": bad["declaration"]["classic_root"] = "/foreign/classic"
            elif change == "history": bad["steps"][0]["request"]["injected"] = True
            elif change == "order": bad["steps"].reverse()
            elif change == "generation": bad["steps"][1]["generation"] = 10
            else: bad["steps"][1]["stage"] = "topology"
            with self.subTest(change=change):
                self.assert_rejected(messages[change], lambda bad=bad: advance.require_append(before, bad))

    def test_envelope_requires_exact_version_and_stage_evidence_shapes(self):
        for mutation, message in (
            (lambda value: value.__setitem__("schema_version", True),
             "retained advance envelope version is unsupported"),
            (lambda value: value["steps"][0].__setitem__("previous_topology_current", {}),
             "retained advance previous topology observation differs from stage"),
            (lambda value: value["steps"][0].__setitem__("request", []),
             "retained advance step evidence is invalid"),
        ):
            bad = self.envelope()
            mutation(bad)
            with self.subTest(message=message):
                self.assert_rejected(message, lambda bad=bad: advance.validate_envelope(bad))
        bad = self.envelope()
        bad["unexpected"] = None
        self.assert_rejected("retained advance envelope has an unsupported shape", lambda: advance.validate_envelope(bad))
        bad = self.envelope()
        for stage, generation, previous in (("plan", 11, None), ("built", 12, None), ("topology", 13, {}),
                                            ("unexpected", 14, None)):
            bad["steps"].append({"stage": stage, "generation": generation, "predecessor_sha256": "f" * 64,
                                 "request": {}, "observations": {}, "previous_topology_current": previous})
        self.assert_rejected("retained advance steps exceed the bounded lifecycle", lambda: advance.validate_envelope(bad))

        bad = self.envelope()
        bad["steps"].append({"stage": "plan", "generation": 11, "predecessor_sha256": "f" * 64,
                             "request": {}, "observations": {}, "previous_topology_current": "untrusted"})
        self.assert_rejected("retained advance has unrelated previous topology evidence",
                             lambda: advance.validate_envelope(bad))

        bad = self.envelope()
        bad["steps"].append({"stage": "plan", "generation": 11, "predecessor_sha256": "f" * 64,
                             "request": {}, "observations": {}, "previous_topology_current": None})
        self.assert_rejected("retained advance must begin with a declaration", lambda: advance.require_append(None, bad))

    def test_topology_history_is_preserved_only_at_topology_stage(self):
        envelope = self.envelope()
        for stage, generation in (("plan", 11), ("built", 12)):
            envelope["steps"].append({"stage": stage, "generation": generation,
                                      "predecessor_sha256": "f" * 64, "request": {}, "observations": {},
                                      "previous_topology_current": None})
        envelope["steps"].append({"stage": "topology", "generation": 13,
                                  "predecessor_sha256": "f" * 64, "request": {}, "observations": {},
                                  "previous_topology_current": {"generation": 9}})
        self.assertEqual(advance.validate_envelope(envelope), envelope)
        missing_history = copy.deepcopy(envelope)
        missing_history["steps"][-1]["previous_topology_current"] = None
        self.assert_rejected("retained advance previous topology observation differs from stage",
                             lambda: advance.validate_envelope(missing_history))

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

    def test_plan_rejects_recomputed_semantic_coordinate_and_role_substitutions(self):
        cases = (
            (lambda value: value["checkout_states"]["classic"].__setitem__("path", "/foreign/classic"),
             "retained advance build plan has unverified Classic/source heads"),
            (lambda value: value["checkout_states"].__setitem__("producer", {"dirty": True}),
             "retained advance build plan has unverified Classic/source heads"),
            (lambda value: value.__setitem__("manifest", {}),
             "retained advance build plan lacks manifest"),
            (lambda value: value["execution_sources"].__setitem__("image", "/substituted/image"),
             "retained advance build roles differ"),
        )
        for mutation, message in cases:
            bad = self.plan()
            mutation(bad)
            bad["plan_sha256"] = advance.digest({key: val for key, val in bad.items() if key != "plan_sha256"})
            with self.subTest(message=message):
                self.assert_rejected(message, lambda bad=bad: self.check_plan(bad))
        bad = self.plan()
        del bad["git_observations"]
        self.assert_rejected("retained advance public build plan has an unsupported shape", lambda: self.check_plan(bad))


if __name__ == "__main__":
    unittest.main()
