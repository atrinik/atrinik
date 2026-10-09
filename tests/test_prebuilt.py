# Copyright 2026 The Atrinik Project
"""Completion evidence rejects malformed receipts and changing runtime inputs."""
import copy
import hashlib
import json
import os
from pathlib import Path
import shutil
import tempfile
import sys
import unittest
from unittest import mock

from atrinik_workspace import prebuilt
from atrinik_workspace.model import WorkspaceError


def canonical(value):
    return json.dumps(value, sort_keys=True, separators=(",", ":"), allow_nan=False).encode()


@unittest.skipUnless(sys.platform == "linux", "completion receipts require Linux")
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

    def test_root_owner_is_required_even_without_receipt(self):
        with mock.patch.object(prebuilt.os, "geteuid", return_value=os.geteuid() + 1):
            with self.assertRaisesRegex(WorkspaceError, "build root must be owned"):
                prebuilt.invalidate(self.root)
            with self.assertRaisesRegex(WorkspaceError, "build root must be owned"):
                self.publish()
        self.root.chmod(0o770)
        prebuilt.invalidate(self.root)

    def test_ancestor_rename_before_open_returns_rejects_invalidation(self):
        coordinate = self.root / "coordinate"
        target = coordinate / "build"
        target.mkdir(parents=True)
        original_receipt = target / prebuilt.RECEIPT_NAME
        original_receipt.write_bytes(b"original")
        original_receipt.chmod(0o600)
        retired = self.root / "retired"
        original_open = os.open
        changed = False

        def racing_open(path, flags, *args, **kwargs):
            nonlocal changed
            fd = original_open(path, flags, *args, **kwargs)
            if not changed and os.readlink(f"/proc/self/fd/{fd}") == str(target):
                changed = True
                coordinate.rename(retired)
                target.mkdir(parents=True)
                replacement = target / prebuilt.RECEIPT_NAME
                replacement.write_bytes(b"replacement")
                replacement.chmod(0o600)
            return fd

        with mock.patch.object(prebuilt.os, "open", side_effect=racing_open):
            with self.assertRaisesRegex(WorkspaceError, "coordinate changed"):
                prebuilt.invalidate(target)
        self.assertEqual(original_receipt.read_bytes(), b"replacement")
        self.assertEqual((retired / "build" / prebuilt.RECEIPT_NAME).read_bytes(), b"original")

    def test_ancestor_rename_during_invalidation_does_not_report_success(self):
        coordinate = self.root / "coordinate"
        target = coordinate / "build"
        target.mkdir(parents=True)
        receipt = target / prebuilt.RECEIPT_NAME
        receipt.write_bytes(b"original")
        receipt.chmod(0o600)
        retired = self.root / "retired"
        original_rename = prebuilt._rename_noreplace

        def racing_rename(directory, source, destination):
            result = original_rename(directory, source, destination)
            if source == prebuilt.RECEIPT_NAME:
                coordinate.rename(retired)
                target.mkdir(parents=True)
                replacement = target / prebuilt.RECEIPT_NAME
                replacement.write_bytes(b"replacement")
                replacement.chmod(0o600)
            return result

        with mock.patch.object(prebuilt, "_rename_noreplace", side_effect=racing_rename):
            with self.assertRaisesRegex(WorkspaceError, "coordinate changed"):
                prebuilt.invalidate(target)
        self.assertEqual(receipt.read_bytes(), b"replacement")

    def test_missing_root_cannot_hide_detached_parent(self):
        parent = self.root / "parent"
        parent.mkdir()
        retired = self.root / "retired"
        original_open = os.open

        def racing_open(path, flags, *args, **kwargs):
            if path == "missing":
                parent.rename(retired)
            return original_open(path, flags, *args, **kwargs)

        with mock.patch.object(prebuilt.os, "open", side_effect=racing_open):
            with self.assertRaisesRegex(WorkspaceError, "coordinate changed"):
                prebuilt.invalidate(parent / "missing")

    def test_input_ancestor_rename_during_open_rejects_capture(self):
        parent = self.root / "parent"
        source = parent / "input"
        parent.mkdir()
        source.write_bytes(b"input")
        retired = self.root / "retired"
        original_open = os.open
        changed = False

        def racing_open(path, flags, *args, **kwargs):
            nonlocal changed
            fd = original_open(path, flags, *args, **kwargs)
            if not changed and os.readlink(f"/proc/self/fd/{fd}") == str(source):
                changed = True
                parent.rename(retired)
                parent.mkdir()
                source.write_bytes(b"input")
            return fd

        selected = {"input": {"path": str(source), "kind": "file", "exclusions": []}}
        with mock.patch.object(prebuilt.os, "open", side_effect=racing_open):
            with self.assertRaisesRegex(WorkspaceError, "coordinate changed"):
                prebuilt.capture_inputs(selected)

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

    def test_relocated_build_root_is_rejected(self):
        sha = self.publish()
        moved = self.root / "another"
        moved.mkdir()
        (moved / prebuilt.RECEIPT_NAME).write_bytes(self.receipt.read_bytes())
        (moved / prebuilt.RECEIPT_NAME).chmod(0o600)
        with self.assertRaises(WorkspaceError):
            prebuilt.load(moved, sha)

    def test_same_path_storage_move_accepts_current_and_legacy_receipts(self):
        for legacy in (False, True):
            with self.subTest(legacy=legacy):
                sha = self.publish()
                value = prebuilt.load(self.root, sha)
                self.assertEqual(value["build_root"], {"path": str(self.root)})
                if legacy:
                    value["build_root"].update(device=self.root.stat().st_dev,
                                               inode=self.root.stat().st_ino)
                    sha = self.raw(canonical(value) + b"\n")
                raw = self.receipt.read_bytes()
                before = self.root.stat()
                with tempfile.TemporaryDirectory() as archive:
                    retired = Path(archive) / "original"
                    self.root.rename(retired)
                    shutil.copytree(retired, self.root)
                    self.assertNotEqual(self.root.stat().st_ino, before.st_ino)
                    loaded = prebuilt.load(self.root, sha)
                    self.assertEqual(loaded, value)
                    prebuilt.verify_inputs(self.selectors, loaded["inputs"])
                    self.assertEqual(self.receipt.read_bytes(), raw)
                prebuilt.invalidate(self.root)

    def test_publish_rejects_root_inode_change_before_installation(self):
        original_identity = prebuilt._root_identity
        changed = False
        with tempfile.TemporaryDirectory() as archive:
            def replacing_identity(root):
                nonlocal changed
                result = original_identity(root)
                if not changed:
                    changed = True
                    self.root.rename(Path(archive) / "original")
                    self.root.mkdir()
                return result

            with mock.patch.object(prebuilt, "_root_identity", side_effect=replacing_identity):
                with self.assertRaisesRegex(WorkspaceError, "changed before publication"):
                    self.publish()
            self.assertFalse(self.receipt.exists())

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

    def test_invalidation_claim_retains_replacement_inode(self):
        self.publish()
        original_rename = prebuilt._rename_noreplace
        replacement_inode = None

        def racing_rename(directory, source, destination):
            nonlocal replacement_inode
            replacement = self.root / "replacement"
            replacement.write_bytes(b"replacement")
            replacement.chmod(0o600)
            replacement_inode = replacement.stat().st_ino
            replacement.replace(self.receipt)
            original_rename(directory, source, destination)

        with mock.patch.object(prebuilt, "_rename_noreplace", side_effect=racing_rename):
            with mock.patch.object(prebuilt.os, "unlink", side_effect=AssertionError("must never unlink")):
                with self.assertRaisesRegex(WorkspaceError, "changed during claim"):
                    prebuilt.invalidate(self.root)
        retained = list(self.root.glob(".atrinik-prebuilt-retired-*"))
        self.assertEqual(len(retained), 1)
        self.assertEqual(retained[0].stat().st_ino, replacement_inode)
        self.assertEqual(retained[0].read_bytes(), b"replacement")
        self.assertFalse(self.receipt.exists())

    def test_invalidation_preserves_replacement_after_claim(self):
        self.publish()
        original_inode = self.receipt.stat().st_ino
        original_rename = prebuilt._rename_noreplace

        def racing_rename(directory, source, destination):
            original_rename(directory, source, destination)
            self.receipt.write_bytes(b"new receipt")
            self.receipt.chmod(0o600)

        with mock.patch.object(prebuilt, "_rename_noreplace", side_effect=racing_rename):
            with mock.patch.object(prebuilt.os, "unlink", side_effect=AssertionError("must never unlink")):
                with self.assertRaisesRegex(WorkspaceError, "replaced after claim"):
                    prebuilt.invalidate(self.root)
        self.assertEqual(self.receipt.read_bytes(), b"new receipt")
        retained = list(self.root.glob(".atrinik-prebuilt-retired-*"))
        self.assertEqual(retained[0].stat().st_ino, original_inode)

    def test_invalidation_quarantine_collision_is_bounded(self):
        self.publish()
        with mock.patch.object(prebuilt, "_rename_noreplace", side_effect=FileExistsError) as rename:
            with self.assertRaisesRegex(WorkspaceError, "quarantine name"):
                prebuilt.invalidate(self.root)
        self.assertEqual(rename.call_count, 8)
        self.assertTrue(self.receipt.is_file())

    def test_publish_caught_fsync_failure_retracts_receipt(self):
        original_fsync = os.fsync
        failures = 0

        def failing_fsync(fd):
            nonlocal failures
            if os.readlink(f"/proc/self/fd/{fd}") == str(self.root) and failures == 0:
                failures += 1
                raise OSError("injected directory fsync failure")
            return original_fsync(fd)

        with mock.patch.object(prebuilt.os, "fsync", side_effect=failing_fsync):
            with self.assertRaises(WorkspaceError):
                self.publish()
        self.assertFalse(self.receipt.exists())
        retained = list(self.root.glob(".atrinik-prebuilt-retired-*"))
        self.assertEqual(len(retained), 1)
        sha = hashlib.sha256(retained[0].read_bytes()).hexdigest()
        with self.assertRaises(WorkspaceError):
            prebuilt.load(self.root, sha)

    def test_publish_write_and_flush_failures_remove_pending_file(self):
        original_fdopen = os.fdopen
        for operation in ("write", "flush"):
            for failure in (OSError, KeyboardInterrupt):
                with self.subTest(operation=operation, failure=failure):
                    def failing_fdopen(fd, mode):
                        stream = original_fdopen(fd, mode)
                        wrapped = mock.MagicMock(wraps=stream)
                        wrapped.__enter__.return_value = wrapped
                        wrapped.__exit__.side_effect = stream.__exit__

                        def fail(*args):
                            stream.write(b"partial receipt")
                            stream.flush()
                            raise failure("injected pending write failure")

                        getattr(wrapped, operation).side_effect = fail
                        return wrapped

                    with mock.patch.object(prebuilt.os, "fdopen", side_effect=failing_fdopen):
                        with self.assertRaises(WorkspaceError if failure is OSError else failure):
                            self.publish()
                    self.assertFalse(self.receipt.exists())
                    self.assertEqual(list(self.root.glob(".atrinik-prebuilt-pending-*")), [])
                    self.assertEqual(list(self.root.glob(".atrinik-prebuilt-retired-*")), [])

    def test_publish_file_fsync_failure_removes_pending_file(self):
        original_fsync = os.fsync

        def failing_fsync(fd):
            if ".atrinik-prebuilt-pending-" in os.readlink(f"/proc/self/fd/{fd}"):
                raise OSError("injected file fsync failure")
            return original_fsync(fd)

        with mock.patch.object(prebuilt.os, "fsync", side_effect=failing_fsync):
            with self.assertRaises(WorkspaceError):
                self.publish()
        self.assertFalse(self.receipt.exists())
        self.assertEqual(list(self.root.glob(".atrinik-prebuilt-pending-*")), [])
        self.assertEqual(list(self.root.glob(".atrinik-prebuilt-retired-*")), [])

    def test_publish_install_failure_removes_pending_and_preserves_receipt(self):
        self.receipt.write_bytes(b"existing receipt")
        self.receipt.chmod(0o600)
        existing_inode = self.receipt.stat().st_ino
        with self.assertRaises(WorkspaceError):
            self.publish()
        self.assertEqual(self.receipt.read_bytes(), b"existing receipt")
        self.assertEqual(self.receipt.stat().st_ino, existing_inode)
        self.assertEqual(list(self.root.glob(".atrinik-prebuilt-pending-*")), [])
        self.assertEqual(list(self.root.glob(".atrinik-prebuilt-retired-*")), [])

    def test_publish_failed_install_preserves_known_pending_replacement(self):
        def failing_rename(directory, source, destination):
            replacement = self.root / "replacement"
            replacement.write_bytes(b"concurrent pending")
            replacement.chmod(0o600)
            replacement.replace(self.root / source)
            raise OSError("injected rename failure")

        with mock.patch.object(prebuilt, "_rename_noreplace", side_effect=failing_rename):
            with self.assertRaises(WorkspaceError):
                self.publish()
        retained = list(self.root.glob(".atrinik-prebuilt-pending-*"))
        self.assertEqual(len(retained), 1)
        self.assertEqual(retained[0].read_bytes(), b"concurrent pending")

    def test_publish_pending_cleanup_claim_preserves_racing_replacement(self):
        original_rename = prebuilt._rename_noreplace

        def racing_rename(directory, source, destination):
            if destination == prebuilt.RECEIPT_NAME:
                raise OSError("injected rename failure")
            replacement = self.root / "replacement"
            replacement.write_bytes(b"racing pending")
            replacement.chmod(0o600)
            replacement.replace(self.root / source)
            original_rename(directory, source, destination)

        with mock.patch.object(prebuilt, "_rename_noreplace", side_effect=racing_rename):
            with self.assertRaisesRegex(WorkspaceError, "rollback evidence"):
                self.publish()
        retained = list(self.root.glob(".atrinik-prebuilt-retired-*"))
        self.assertEqual(len(retained), 1)
        self.assertEqual(retained[0].read_bytes(), b"racing pending")
        self.assertEqual(list(self.root.glob(".atrinik-prebuilt-pending-*")), [])

    def test_publish_caught_baseexception_retracts_receipt(self):
        original_fsync = os.fsync
        failures = 0

        def interrupted_fsync(fd):
            nonlocal failures
            if os.readlink(f"/proc/self/fd/{fd}") == str(self.root) and failures == 0:
                failures += 1
                raise KeyboardInterrupt
            return original_fsync(fd)

        with mock.patch.object(prebuilt.os, "fsync", side_effect=interrupted_fsync):
            with self.assertRaises(KeyboardInterrupt):
                self.publish()
        self.assertFalse(self.receipt.exists())
        self.assertEqual(len(list(self.root.glob(".atrinik-prebuilt-retired-*"))), 1)

    def test_publish_failed_fsync_preserves_known_replacement(self):
        original_fsync = os.fsync
        changed = False

        def replacing_fsync(fd):
            nonlocal changed
            if not changed and os.readlink(f"/proc/self/fd/{fd}") == str(self.root):
                changed = True
                replacement = self.root / "replacement"
                replacement.write_bytes(b"concurrent receipt")
                replacement.chmod(0o600)
                replacement.replace(self.receipt)
                raise OSError("injected failure")
            return original_fsync(fd)

        with mock.patch.object(prebuilt.os, "fsync", side_effect=replacing_fsync):
            with self.assertRaises(WorkspaceError):
                self.publish()
        self.assertEqual(self.receipt.read_bytes(), b"concurrent receipt")

    def test_publish_rollback_claim_preserves_racing_replacement(self):
        original_fsync = os.fsync
        original_rename = prebuilt._rename_noreplace
        failed = False

        def failing_fsync(fd):
            nonlocal failed
            if not failed and os.readlink(f"/proc/self/fd/{fd}") == str(self.root):
                failed = True
                raise OSError("injected failure")
            return original_fsync(fd)

        def racing_rename(directory, source, destination):
            if source == prebuilt.RECEIPT_NAME:
                replacement = self.root / "replacement"
                replacement.write_bytes(b"racing receipt")
                replacement.chmod(0o600)
                replacement.replace(self.receipt)
            original_rename(directory, source, destination)

        with mock.patch.object(prebuilt.os, "fsync", side_effect=failing_fsync):
            with mock.patch.object(prebuilt, "_rename_noreplace", side_effect=racing_rename):
                with self.assertRaises(WorkspaceError):
                    self.publish()
        retained = list(self.root.glob(".atrinik-prebuilt-retired-*"))
        self.assertEqual(len(retained), 1)
        self.assertEqual(retained[0].read_bytes(), b"racing receipt")
        self.assertFalse(self.receipt.exists())

    def test_publish_interrupted_install_call_still_retracts_receipt(self):
        original_rename = prebuilt._rename_noreplace

        def interrupted_rename(directory, source, destination):
            original_rename(directory, source, destination)
            if destination == prebuilt.RECEIPT_NAME:
                raise KeyboardInterrupt

        with mock.patch.object(prebuilt, "_rename_noreplace", side_effect=interrupted_rename):
            with self.assertRaises(KeyboardInterrupt):
                self.publish()
        self.assertFalse(self.receipt.exists())

    def test_other_platform_builds_require_absent_receipt(self):
        for system in ("Windows", "Darwin"):
            with self.subTest(system=system), mock.patch.object(prebuilt.platform, "system", return_value=system):
                prebuilt.invalidate(self.root)
                prebuilt.invalidate(self.root / "missing" / "root")
                for creator in (lambda: self.receipt.write_bytes(b"receipt"),
                                lambda: self.receipt.symlink_to(self.executable),
                                lambda: os.mkfifo(self.receipt)):
                    creator()
                    try:
                        with self.assertRaisesRegex(WorkspaceError, "retirement on Linux"):
                            prebuilt.invalidate(self.root)
                    finally:
                        self.receipt.unlink()
                link = self.root / "linked-root"
                link.symlink_to(self.assets)
                try:
                    with self.assertRaises(WorkspaceError):
                        prebuilt.invalidate(link)
                finally:
                    link.unlink()

    def test_publish_rejects_replaced_pending_inode(self):
        original_rename = prebuilt._rename_noreplace

        def racing_rename(directory, source, destination):
            if destination == prebuilt.RECEIPT_NAME:
                replacement = self.root / "replacement"
                replacement.write_bytes(b"replacement")
                replacement.chmod(0o600)
                replacement.replace(self.root / source)
            original_rename(directory, source, destination)

        with mock.patch.object(prebuilt, "_rename_noreplace", side_effect=racing_rename):
            with self.assertRaisesRegex(WorkspaceError, "changed during publication"):
                self.publish()
        self.assertEqual(self.receipt.read_bytes(), b"replacement")

    def test_read_metadata_strict_bounded_nonblocking(self):
        metadata = self.root / "metadata.json"
        metadata.write_text('{ "schema_version": 1 }\n')
        self.assertEqual(prebuilt.read_metadata(metadata, "fixture"), {"schema_version": 1})
        invalid = [b'{"key":1,"key":2}', b'{"key":NaN}', b'{"key":1e999}', b'[]', b'{']
        for raw in invalid:
            metadata.write_bytes(raw)
            with self.subTest(raw=raw), self.assertRaises(WorkspaceError):
                prebuilt.read_metadata(metadata, "fixture")
        metadata.write_text('{"large":"payload"}')
        with self.assertRaises(WorkspaceError):
            prebuilt.read_metadata(metadata, "fixture", limit=4)
        metadata.unlink()
        for creator in (lambda: os.mkfifo(metadata), lambda: metadata.symlink_to(self.executable),
                        lambda: os.link(self.executable, metadata)):
            creator()
            try:
                with self.assertRaises(WorkspaceError):
                    prebuilt.read_metadata(metadata, "fixture")
            finally:
                metadata.unlink()

    def test_read_metadata_rejects_content_mutation(self):
        metadata = self.root / "metadata.json"
        metadata.write_text('{"a":1}')
        original_read = os.read
        changed = False

        def racing_read(fd, count):
            nonlocal changed
            value = original_read(fd, count)
            if not changed:
                changed = True
                metadata.write_text('{"a":2}')
            return value

        with mock.patch.object(prebuilt.os, "read", side_effect=racing_read):
            with self.assertRaises(WorkspaceError):
                prebuilt.read_metadata(metadata, "fixture")


if __name__ == "__main__":
    unittest.main()
