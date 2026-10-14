# Copyright (c) 2026 Atrinik contributors
# SPDX-License-Identifier: MIT
"""Private finite-tree fixtures; no host shell or fixture payload is executed."""
from contextlib import ExitStack
import copy
import hashlib
import os
from pathlib import Path
import stat
import struct
import tempfile
from types import SimpleNamespace
import unittest
from unittest import mock

from atrinik_workspace import linux_portable_shell as shell


class ShellFixture:
    """Only a private fixture root/UID view and synthetic byte pin are substituted."""
    def __init__(self):
        self.stack = ExitStack()
        self.temporary = self.stack.enter_context(tempfile.TemporaryDirectory())
        self.root = Path(self.temporary) / "root"
        self.root.mkdir(mode=0o755)
        (self.root / "usr").mkdir(mode=0o755)
        (self.root / "usr/bin").mkdir(mode=0o755)
        for directory in (self.root, self.root / "usr", self.root / "usr/bin"):
            directory.chmod(0o755)
        (self.root / "bin").symlink_to("usr/bin")
        (self.root / "usr/bin/sh").symlink_to("dash")
        self.target = self.root / "usr/bin/dash"
        raw = bytearray(128)
        raw[:7] = b"\x7fELF\x02\x01\x01"
        struct.pack_into("<HHI", raw, 16, 3, 62, 1)
        struct.pack_into("<H", raw, 52, 64)
        self.raw = bytes(raw)
        self.target.write_bytes(self.raw)
        self.target.chmod(0o755)
        self.real_open, self.real_stat = os.open, os.stat
        self.real_fstat, self.real_read = os.fstat, os.read
        self.uid_overrides = {}
        self.gid_overrides = {}
        self.stack.enter_context(mock.patch.object(shell, "_open_root", side_effect=self.open_root))
        self.stack.enter_context(mock.patch.object(shell, "_SIZE", len(self.raw)))
        self.stack.enter_context(mock.patch.object(shell, "_SHA256", hashlib.sha256(self.raw).hexdigest()))
        self.stack.enter_context(mock.patch.object(shell.os, "stat", side_effect=self.fixture_stat))
        self.stack.enter_context(mock.patch.object(shell.os, "fstat", side_effect=self.fixture_fstat))

    def close(self):
        self.stack.close()

    def open_root(self):
        return self.real_open(self.root, os.O_RDONLY | os.O_DIRECTORY | os.O_NOFOLLOW | os.O_CLOEXEC)

    def normalized(self, info):
        values = {key: getattr(info, key) for key in dir(info) if key.startswith("st_")}
        values.update(st_uid=self.uid_overrides.get(info.st_ino, 0),
                      st_gid=self.gid_overrides.get(info.st_ino, 0))
        return SimpleNamespace(**values)

    def fixture_stat(self, *args, **kwargs):
        return self.normalized(self.real_stat(*args, **kwargs))

    def fixture_fstat(self, fd):
        return self.normalized(self.real_fstat(fd))

    def during_read(self, action):
        used = False
        def read(fd, count):
            nonlocal used
            data = self.real_read(fd, count)
            if not used:
                used = True
                action()
            return data
        return mock.patch.object(shell.os, "read", side_effect=read)


class PortableShellTests(unittest.TestCase):
    def setUp(self):
        self.fixture = ShellFixture()
        self.addCleanup(self.fixture.close)

    def reject(self, action=None):
        if action:
            action()
        with self.assertRaises(shell.LinuxPortableShellError):
            shell.capture_identity()

    def test_capture_and_same_operation_verify(self):
        receipt = shell.capture_identity()
        shell.verify_identity(copy.deepcopy(receipt))
        self.assertEqual(receipt["path"], "/bin/sh")
        self.assertEqual(receipt["resolved_path"], "/usr/bin/dash")
        self.assertEqual(set(receipt["entries"]), set(shell._ENTRIES))
        self.assertEqual(receipt["entries"]["/bin"]["link_target"], "usr/bin")
        self.assertEqual(receipt["entries"]["/usr/bin/sh"]["link_target"], "dash")

    def test_literal_is_bound_to_actual_collector(self):
        self.assertEqual(shell._PROVENANCE["run_id"], 38525579823)
        self.assertEqual(shell._PROVENANCE["artifact_id"], 11874924218)
        self.assertEqual(shell._cohort()["config_digest"],
            "sha256:c35dbeac2679d79dab524a13c9a3b74a8187878ecd3b3dff52bd2892418d7a79")

    def test_cohort_exact_and_each_changed_coordinate(self):
        good = dict(image=shell._IMAGE, platform_manifest=shell._PLATFORM,
                    producer_commit=shell._PRODUCER, consumer_commit=shell._CONSUMER)
        shell.verify_cohort(**good)
        for key in good:
            with self.subTest(key=key), self.assertRaises(shell.LinuxPortableShellError):
                shell.verify_cohort(**{**good, key: "changed"})

    def test_root_owner_is_checked_without_fixture_normalization(self):
        info = self.fixture.normalized(self.fixture.real_stat(self.fixture.root))
        info.st_uid = 1000
        with self.assertRaises(shell.LinuxPortableShellError):
            shell._metadata(info, "/")

    def test_each_entry_rejects_nonroot_owner(self):
        for path in shell._ENTRIES:
            with self.subTest(path=path):
                local = self.fixture.root / path.lstrip("/")
                inode = self.fixture.real_stat(local, follow_symlinks=False).st_ino
                self.fixture.uid_overrides[inode] = 1000
                self.reject()
                self.fixture.uid_overrides.clear()

    def test_nonroot_group(self):
        inode = self.fixture.real_stat(self.fixture.target).st_ino
        self.fixture.gid_overrides[inode] = 1000
        self.reject()

    def test_writable_ancestor(self):
        self.reject(lambda: (self.fixture.root / "usr").chmod(0o775))

    def test_wrong_root_mode(self):
        self.reject(lambda: self.fixture.root.chmod(0o1777))

    def test_writable_target(self):
        self.reject(lambda: self.fixture.target.chmod(0o777))

    def test_setuid_target(self):
        self.reject(lambda: self.fixture.target.chmod(0o4755))

    def test_nonexecutable_target(self):
        self.reject(lambda: self.fixture.target.chmod(0o644))

    def test_hardlinked_target(self):
        os.link(self.fixture.target, self.fixture.root / "alias")
        self.reject()

    def test_target_symlink_alias(self):
        self.fixture.target.rename(self.fixture.root / "other")
        self.fixture.target.symlink_to("../../other")
        self.reject()

    def test_directory_symlink_alias(self):
        binary = self.fixture.root / "usr/bin"
        binary.rename(self.fixture.root / "actual-bin")
        binary.symlink_to("../actual-bin")
        self.reject()

    def test_missing_entry(self):
        (self.fixture.root / "usr/bin/sh").unlink()
        self.reject()

    def test_wrong_bin_link_text(self):
        (self.fixture.root / "bin").unlink()
        (self.fixture.root / "bin").symlink_to("/usr/bin")
        self.reject()

    def test_wrong_shell_link_text(self):
        (self.fixture.root / "usr/bin/sh").unlink()
        (self.fixture.root / "usr/bin/sh").symlink_to("other")
        self.reject()

    def test_special_target_does_not_block(self):
        self.fixture.target.unlink()
        os.mkfifo(self.fixture.target, 0o755)
        self.reject()

    def test_target_size_drift(self):
        self.fixture.target.write_bytes(self.fixture.raw + b"extra")
        self.reject()

    def test_target_byte_drift(self):
        self.fixture.target.write_bytes(self.fixture.raw[:-1] + b"x")
        self.reject()

    def test_wrong_elf_header_even_with_matching_fixture_hash(self):
        raw = bytearray(self.fixture.raw)
        struct.pack_into("<H", raw, 18, 183)
        self.fixture.target.write_bytes(raw)
        with mock.patch.object(shell, "_SHA256", hashlib.sha256(raw).hexdigest()):
            self.reject()

    def test_replaced_target_same_bytes_between_capture_and_verify(self):
        receipt = shell.capture_identity()
        self.fixture.target.rename(self.fixture.root / "old")
        self.fixture.target.write_bytes(self.fixture.raw)
        self.fixture.target.chmod(0o755)
        with self.assertRaises(shell.LinuxPortableShellError):
            shell.verify_identity(receipt)

    def test_changed_bytes_between_capture_and_verify(self):
        receipt = shell.capture_identity()
        self.fixture.target.write_bytes(self.fixture.raw[:-1] + b"x")
        with self.assertRaises(shell.LinuxPortableShellError):
            shell.verify_identity(receipt)

    def test_target_path_swap_during_read(self):
        def change():
            self.fixture.target.rename(self.fixture.root / "old")
            self.fixture.target.write_bytes(self.fixture.raw)
            self.fixture.target.chmod(0o755)
        with self.fixture.during_read(change):
            self.reject()

    def test_link_replacement_during_read(self):
        def change():
            path = self.fixture.root / "usr/bin/sh"
            path.unlink()
            path.symlink_to("dash")
        with self.fixture.during_read(change):
            self.reject()

    def test_ancestor_replacement_during_read(self):
        def change():
            (self.fixture.root / "usr").rename(self.fixture.root / "old-usr")
            (self.fixture.root / "usr").mkdir(mode=0o755)
        with self.fixture.during_read(change):
            self.reject()

    def test_inode_metadata_drift_during_read(self):
        with self.fixture.during_read(lambda: self.fixture.target.chmod(0o700)):
            self.reject()

    def test_receipt_schema_boolean_rejected(self):
        receipt = shell.capture_identity()
        receipt["schema_version"] = True
        with self.assertRaises(shell.LinuxPortableShellError):
            shell.verify_identity(receipt)

    def test_changed_receipt_cohort_rejected(self):
        receipt = shell.capture_identity()
        receipt["cohort"]["image"] = "other"
        with self.assertRaises(shell.LinuxPortableShellError):
            shell.verify_identity(receipt)

    def test_missing_and_non_json_receipt(self):
        good = shell.capture_identity()
        bad_inode = copy.deepcopy(good)
        bad_inode["entries"]["/usr/bin/dash"]["identity"][1] += 1
        tuple_identity = copy.deepcopy(good)
        tuple_identity["entries"]["/usr/bin/dash"]["identity"] = tuple(
            tuple_identity["entries"]["/usr/bin/dash"]["identity"])
        for receipt in (None, {}, {**good, "unexpected": 1},
                        {**good, "size": float("nan")}, bad_inode, tuple_identity):
            with self.subTest(receipt=type(receipt)), self.assertRaises(shell.LinuxPortableShellError):
                shell.verify_identity(receipt)

    def test_unsupported_platform_fails_before_filesystem_access(self):
        for platform in ("win32", "darwin"):
            with self.subTest(platform=platform), mock.patch.object(shell.sys, "platform", platform), \
                    mock.patch.object(shell, "_open_root") as opened:
                with self.assertRaisesRegex(shell.LinuxPortableShellError,
                                            "Linux descriptor capabilities required"):
                    shell.capture_identity()
                opened.assert_not_called()

    def test_missing_descriptor_capability_fails_before_filesystem_access(self):
        for name in ("O_PATH", "O_NOFOLLOW", "O_DIRECTORY", "O_CLOEXEC", "O_NONBLOCK", "O_RDONLY"):
            with self.subTest(flag=name), mock.patch.object(shell.os, name), \
                    mock.patch.object(shell, "_open_root") as opened:
                delattr(shell.os, name)
                with self.assertRaisesRegex(shell.LinuxPortableShellError,
                                            "Linux descriptor capabilities required"):
                    shell.capture_identity()
                opened.assert_not_called()

    def test_import_and_cohort_remain_available_on_unsupported_platform(self):
        code = compile(Path(shell.__file__).read_bytes(), shell.__file__, "exec")
        namespace = {}
        with mock.patch.object(shell.sys, "platform", "win32"):
            exec(code, namespace)
            namespace["verify_cohort"](image=namespace["_IMAGE"],
                platform_manifest=namespace["_PLATFORM"], producer_commit=namespace["_PRODUCER"],
                consumer_commit=namespace["_CONSUMER"])
            self.assertTrue(issubclass(namespace["LinuxPortableShellError"], ValueError))
            with mock.patch.dict(namespace, {"_open_root": mock.Mock()}) as patched:
                with self.assertRaises(namespace["LinuxPortableShellError"]):
                    namespace["capture_identity"]()
                patched["_open_root"].assert_not_called()

    def test_descriptor_closed_on_failure(self):
        opened = []
        def track(*args, **kwargs):
            fd = self.fixture.real_open(*args, **kwargs)
            opened.append(fd)
            return fd
        with mock.patch.object(shell.os, "open", side_effect=track), \
                mock.patch.object(shell, "_SHA256", "0" * 64):
            self.reject()
        self.assertTrue(opened)
        for fd in opened:
            with self.assertRaises(OSError):
                self.fixture.real_fstat(fd)


if __name__ == "__main__":
    unittest.main()
