# Copyright 2026 The Atrinik Project
"""Completion evidence rejects malformed receipts and changing runtime inputs."""
import copy
import hashlib
import json
import os
from pathlib import Path
import tempfile
import unittest
from unittest import mock

from atrinik_workspace import prebuilt
from atrinik_workspace.model import WorkspaceError


def canonical(value):
    return json.dumps(value, sort_keys=True, separators=(",", ":"), allow_nan=False).encode()


class PrebuiltReceiptTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name)
        self.assets = self.root / "assets"
        self.assets.mkdir()
        self.executable = self.assets / "server"
        self.executable.write_bytes(b"runtime bytes")
        self.executable.chmod(0o700)
        (self.assets / "map").write_text("world")
        self.selectors = {"runtime": {"path": str(self.assets), "kind": "tree", "exclusions": []}}
        self.inputs = prebuilt.capture_inputs(self.selectors)
        self.plan = {
            "schema_version": 1, "target": "topology", "profile": {"stack": "classic"},
            "tests": True, "force_reconfigure": False, "use_ccache": True,
            "targets": ["server", "client"], "manifest": {}, "checkout_states": {},
            "source_fingerprints": {}, "git_observations": {}, "sources": {},
            "execution_sources": {}, "wrapper_root": str(self.root),
            "workspace_root": str(self.root), "builds_root": str(self.root),
            "build_key": "abcdef", "build_root": str(self.root),
        }
        self.plan["plan_sha256"] = hashlib.sha256(canonical(self.plan)).hexdigest()
        self.producer = {"generation": "a" * 64, "system": "Linux", "machine": "x86_64",
                         "wrapper_head": "b" * 40, "configurations": {
                             "build/server/.atrinik-configure.json": {
                                 "schema_version": 1, "purpose": "cmake-configure"}}}
        self.receipt = self.root / prebuilt.RECEIPT_NAME

    def publish(self):
        return prebuilt.publish(self.root, self.plan, self.producer, self.inputs)

    def raw(self, payload):
        self.receipt.write_bytes(payload)
        self.receipt.chmod(0o600)
        return hashlib.sha256(payload).hexdigest()

    def test_roundtrip_invalidate_and_republish(self):
        sha = self.publish()
        value = prebuilt.load(self.root, sha)
        self.assertEqual(value["plan"], self.plan)
        self.assertEqual(value["inputs"], self.inputs)
        self.assertEqual(self.receipt.stat().st_mode & 0o777, 0o600)
        self.assertEqual(self.receipt.stat().st_nlink, 1)
        prebuilt.verify_inputs(self.selectors, value["inputs"])
        with self.assertRaises(WorkspaceError):
            self.publish()
        prebuilt.invalidate(self.root)
        prebuilt.invalidate(self.root)
        self.assertEqual(self.publish(), sha)

    def test_invalidation_missing_root_and_unsafe_ancestors(self):
        absent = self.root / "absent" / "child"
        prebuilt.invalidate(absent)
        self.assertFalse(absent.parent.exists())
        link = self.root / "link"
        for target in (self.root / "missing-target", self.assets):
            link.symlink_to(target)
            try:
                for selected in (link, link / "missing-child"):
                    with self.subTest(selected=selected), self.assertRaises(WorkspaceError):
                        prebuilt.invalidate(selected)
            finally:
                link.unlink()
        fifo = self.root / "fifo"
        os.mkfifo(fifo)
        with self.assertRaises(WorkspaceError):
            prebuilt.invalidate(fifo / "missing-child")

    def test_receipt_foreign_owner_rejected(self):
        sha = self.publish()
        with mock.patch.object(prebuilt.os, "geteuid", return_value=os.geteuid() + 1):
            with self.assertRaises(WorkspaceError):
                prebuilt.load(self.root, sha)
            with self.assertRaises(WorkspaceError):
                prebuilt.invalidate(self.root)

    def test_wrong_digest_and_missing_receipt(self):
        sha = self.publish()
        for invalid in ("f" * 64, "", "A" * 64, True):
            with self.subTest(invalid=invalid), self.assertRaises(WorkspaceError):
                prebuilt.load(self.root, invalid)
        self.receipt.unlink()
        with self.assertRaises(WorkspaceError):
            prebuilt.load(self.root, sha)

    def test_malformed_noncanonical_duplicate_nonfinite(self):
        sha = self.publish()
        value = prebuilt.load(self.root, sha)
        payloads = [b"{", b'{"x":1,"x":2}\n', b'{"x":NaN}\n',
                    b'{"x":Infinity}\n', canonical(value), json.dumps(value).encode() + b"\n",
                    b"[" * 2000 + b"]" * 2000, b"\xff"]
        for payload in payloads:
            with self.subTest(payload=payload[:25]), self.assertRaises(WorkspaceError):
                prebuilt.load(self.root, self.raw(payload))

    def test_exact_schema_plan_and_producer_validation(self):
        sha = self.publish()
        original = prebuilt.load(self.root, sha)
        variants = []
        for key, value in (("schema_version", True), ("unexpected", 1), ("inputs", {})):
            variant = copy.deepcopy(original)
            variant[key] = value
            variants.append(variant)
        for key, value in (("targets", ["server"]), ("targets", [1, "server"]),
                           ("target", "server"), ("tests", 1), ("profile", {"stack": "default"}),
                           ("build_root", "/elsewhere"), ("plan_sha256", "f" * 64)):
            variant = copy.deepcopy(original)
            variant["plan"][key] = value
            variants.append(variant)
        for key, value in (("generation", "bad"), ("wrapper_head", "bad"),
                           ("system", "Windows"), ("machine", ""), ("extra", 1),
                           ("configurations", {"build/../.atrinik-configure.json": {
                               "schema_version": 1, "purpose": "cmake-configure"}})):
            variant = copy.deepcopy(original)
            variant["producer"][key] = value
            variants.append(variant)
        for variant in variants:
            with self.subTest(variant=variant), self.assertRaises(WorkspaceError):
                prebuilt.load(self.root, self.raw(canonical(variant) + b"\n"))

    def test_receipt_special_objects_and_nonprivate_permissions(self):
        target = self.root / "target"
        target.write_text("preserve")
        creators = [lambda: self.receipt.symlink_to(target),
                    lambda: os.link(target, self.receipt),
                    lambda: os.mkfifo(self.receipt), lambda: self.receipt.mkdir()]
        for creator in creators:
            creator()
            try:
                for operation in (lambda: prebuilt.load(self.root, "a" * 64),
                                  lambda: prebuilt.invalidate(self.root), self.publish):
                    with self.subTest(creator=creator, operation=operation), self.assertRaises(WorkspaceError):
                        operation()
                self.assertEqual(target.read_text(), "preserve")
            finally:
                if self.receipt.is_dir():
                    self.receipt.rmdir()
                else:
                    self.receipt.unlink()
        sha = self.publish()
        self.receipt.chmod(0o644)
        with self.assertRaises(WorkspaceError):
            prebuilt.load(self.root, sha)
        with self.assertRaises(WorkspaceError):
            prebuilt.invalidate(self.root)

    def test_relocated_or_replaced_build_root(self):
        sha = self.publish()
        moved = self.root / "another"
        moved.mkdir()
        (moved / prebuilt.RECEIPT_NAME).write_bytes(self.receipt.read_bytes())
        (moved / prebuilt.RECEIPT_NAME).chmod(0o600)
        with self.assertRaises(WorkspaceError):
            prebuilt.load(moved, sha)
        value = prebuilt.load(self.root, sha)
        value["build_root"]["inode"] += 1
        with self.assertRaises(WorkspaceError):
            prebuilt.load(self.root, self.raw(canonical(value) + b"\n"))

    def test_receipt_and_input_size_bounds(self):
        sha = self.publish()
        with mock.patch.object(prebuilt, "MAX_RECEIPT_BYTES", 32):
            with self.assertRaises(WorkspaceError):
                prebuilt.load(self.root, sha)
            with self.assertRaises(WorkspaceError):
                self.publish()
        for constant in ("MAX_ENTRIES", "MAX_INPUT_BYTES"):
            with mock.patch.object(prebuilt, constant, 1), self.assertRaises(WorkspaceError):
                prebuilt.capture_inputs(self.selectors)

    def test_bytes_names_types_and_executable_modes_are_bound(self):
        mutations = [lambda: self.executable.write_bytes(b"changed bytes"),
                     lambda: self.executable.chmod(0o600),
                     lambda: self.executable.rename(self.assets / "renamed"),
                     lambda: (self.assets / "added").mkdir(),
                     lambda: (self.assets / "map").unlink()]
        for mutation in mutations:
            baseline = prebuilt.capture_inputs(self.selectors)
            mutation()
            with self.assertRaises(WorkspaceError):
                prebuilt.verify_inputs(self.selectors, baseline)

    def test_tree_file_symlink_fifo_and_hardlink_rejected(self):
        entry = self.assets / "unsafe"
        for creator in (lambda: entry.symlink_to(self.executable),
                        lambda: os.link(self.executable, entry), lambda: os.mkfifo(entry)):
            creator()
            try:
                with self.assertRaises(WorkspaceError):
                    prebuilt.capture_inputs(self.selectors)
            finally:
                entry.unlink()
        link = self.root / "linked-assets"
        link.symlink_to(self.assets)
        with self.assertRaises(WorkspaceError):
            prebuilt.capture_inputs({"file": {"path": str(link / "server"), "kind": "file", "exclusions": []}})
        with self.assertRaises(WorkspaceError):
            prebuilt.capture_inputs({"file": {"path": str(self.assets), "kind": "file", "exclusions": []}})

    def test_selector_validation_and_immediate_exclusions(self):
        for path in ("relative", str(self.assets) + "/../assets", str(self.assets) + "/."):
            with self.subTest(path=path), self.assertRaises(WorkspaceError):
                prebuilt.capture_inputs({"runtime": {"path": path, "kind": "tree", "exclusions": []}})
        for exclusions in (["../escape"], ["a/b"], ["b", "a"], ["a", "a"], [1], ["."]):
            with self.subTest(exclusions=exclusions), self.assertRaises(WorkspaceError):
                prebuilt.capture_inputs({"runtime": {"path": str(self.assets), "kind": "tree", "exclusions": exclusions}})
        selected = copy.deepcopy(self.selectors)
        selected["runtime"]["exclusions"] = ["map"]
        manifest = prebuilt.capture_inputs(selected)
        (self.assets / "map").write_text("excluded")
        prebuilt.verify_inputs(selected, manifest)
        with self.assertRaises(WorkspaceError):
            prebuilt.verify_inputs(self.selectors, manifest)
        nested = self.assets / "nested"
        nested.mkdir()
        (nested / "map").write_text("included")
        manifest = prebuilt.capture_inputs(selected)
        (nested / "map").write_text("modified")
        with self.assertRaises(WorkspaceError):
            prebuilt.verify_inputs(selected, manifest)

    def test_single_file_capture_and_verification(self):
        selected = {"executable": {"path": str(self.executable), "kind": "file", "exclusions": []}}
        manifest = prebuilt.capture_inputs(selected)
        prebuilt.verify_inputs(selected, manifest)
        self.executable.write_bytes(b"changed")
        with self.assertRaises(WorkspaceError):
            prebuilt.verify_inputs(selected, manifest)

    def test_mutation_during_read_and_between_passes(self):
        original_read = os.read
        changed = False

        def racing_read(fd, count):
            nonlocal changed
            value = original_read(fd, count)
            if not changed and os.readlink(f"/proc/self/fd/{fd}") == str(self.executable):
                changed = True
                self.executable.write_bytes(b"concurrent modification")
            return value

        with mock.patch.object(prebuilt.os, "read", side_effect=racing_read), self.assertRaises(WorkspaceError):
            prebuilt.capture_inputs(self.selectors)
        original_observe = prebuilt._observe

        def racing_observe(selector, budget, **kwargs):
            result = original_observe(selector, budget, **kwargs)
            if kwargs.get("expected") is None:
                self.executable.write_bytes(b"changed after capture")
            return result

        with mock.patch.object(prebuilt, "_observe", side_effect=racing_observe), self.assertRaises(WorkspaceError):
            prebuilt.capture_inputs(self.selectors)

    def test_invalidation_detects_replaced_receipt(self):
        self.publish()
        original_stat = os.stat
        count = 0

        def racing_stat(path, *args, **kwargs):
            nonlocal count
            if path == prebuilt.RECEIPT_NAME:
                count += 1
                if count == 2:
                    replacement = self.root / "replacement"
                    replacement.write_bytes(b"replacement")
                    replacement.chmod(0o600)
                    replacement.replace(self.receipt)
            return original_stat(path, *args, **kwargs)

        with mock.patch.object(prebuilt.os, "stat", side_effect=racing_stat), self.assertRaises(WorkspaceError):
            prebuilt.invalidate(self.root)
        self.assertEqual(self.receipt.read_bytes(), b"replacement")


if __name__ == "__main__":
    unittest.main()
