from __future__ import annotations

import hashlib
import json
import os
from pathlib import Path
import tempfile
import subprocess
import shutil
import sys
import unittest
from unittest import mock

from atrinik_workspace import linux_export as export


@unittest.skipUnless(sys.platform.startswith("linux"), "Linux filesystem and desktop capabilities")
class LinuxExportTests(unittest.TestCase):
    def setUp(self) -> None:
        self.temporary = tempfile.TemporaryDirectory()
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name) / "client"
        self.root.mkdir()
        self.manifest = {"schema_version": 1,
                         "sources": {"atrinik/classic@main": "a" * 40},
                         "application_libraries": ["lib/libapp.so"],
                         "notices": ["licenses/NOTICE"], "files": {}}
        self.add_file("lib/libapp.so", b"library")
        self.add_file("licenses/NOTICE", b"license")
        self.add_file("bin/client", b"executable", executable=True)
        self.write_manifest()

    def add_file(self, name: str, content: bytes, *, executable: bool = False) -> None:
        path = self.root / name
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_bytes(content)
        path.chmod(0o755 if executable else 0o644)
        self.manifest["files"][name] = {"sha256": hashlib.sha256(content).hexdigest(),
                                      "size": len(content), "executable": executable}

    def write_manifest(self) -> None:
        (self.root / export.MANIFEST_NAME).write_text(json.dumps(self.manifest))

    def test_exact_closure_survives_movement_without_original_path(self) -> None:
        target = self.root.parent / "moved folder"
        self.root.rename(target)
        self.assertEqual(export.verify_export(target), self.manifest)

    def test_missing_corrupt_extra_and_wrong_mode_payloads_fail(self) -> None:
        target = self.root / "lib/libapp.so"
        for data in (b"corrupt", b"longer library", b""):
            target.write_bytes(data)
            with self.assertRaises(export.ExportError):
                export.verify_export(self.root)
        target.write_bytes(b"library")
        target.chmod(0o755)
        with self.assertRaises(export.ExportError):
            export.verify_export(self.root)
        target.chmod(0o644)
        extra = self.root / "unexpected"
        extra.write_text("extra")
        with self.assertRaises(export.ExportError):
            export.verify_export(self.root)
        extra.unlink()
        target.unlink()
        with self.assertRaises(export.ExportError):
            export.verify_export(self.root)

    def test_pointer_text_is_rejected_even_with_matching_hash(self) -> None:
        self.add_file("media/image.png", export.LFS_HEADER + b"oid sha256:" + b"a" * 64 + b"\nsize 123\n")
        self.write_manifest()
        with self.assertRaisesRegex(export.ExportError, "export-lfs-pointer"):
            export.verify_export(self.root)

    def test_symlink_payload_and_ancestor_are_rejected(self) -> None:
        original = self.root / "lib/libapp.so"
        original.unlink()
        original.symlink_to(self.root / "licenses/NOTICE")
        with self.assertRaises(export.ExportError):
            export.verify_export(self.root)
        alias = self.root.parent / "alias"
        alias.symlink_to(self.root, target_is_directory=True)
        with self.assertRaises(export.ExportError):
            export.verify_export(alias)

    def test_special_file_does_not_block_the_verifier(self) -> None:
        path = self.root / "lib/libapp.so"
        path.unlink()
        os.mkfifo(path)
        with self.assertRaises(export.ExportError):
            export.verify_export(self.root)

    def test_manifest_duplicate_keys_and_missing_legal_closure_rejected(self) -> None:
        with self.assertRaises(export.ExportError):
            export.load_manifest(b'{"schema_version":1,"schema_version":1}')
        for names in ([], ["missing"], [{}]):
            self.manifest["notices"] = names
            with self.assertRaises(export.ExportError):
                export.load_manifest(json.dumps(self.manifest).encode())

    def test_escaping_and_ambiguous_paths_rejected(self) -> None:
        for name in ("../out", "/out", "a/../b", "a//b", "a/./b", "a\\b", "a\nb", ""):
            with self.subTest(name=name):
                with self.assertRaises(export.ExportError):
                    export.relative_path(name)

    def test_false_schema_and_negative_sizes_rejected(self) -> None:
        self.manifest["schema_version"] = True
        with self.assertRaises(export.ExportError):
            export.load_manifest(json.dumps(self.manifest).encode())
        self.manifest["schema_version"] = 1
        self.manifest["files"]["bin/client"]["size"] = -1
        with self.assertRaises(export.ExportError):
            export.load_manifest(json.dumps(self.manifest).encode())

    def test_setid_payload_is_rejected(self) -> None:
        (self.root / "bin/client").chmod(0o4755)
        with self.assertRaises(export.ExportError):
            export.verify_export(self.root)


    def test_unreadable_directory_fails_instead_of_disappearing(self) -> None:
        private = self.root / "unreadable"
        private.mkdir()
        private.chmod(0)
        try:
            with self.assertRaises(export.ExportError):
                export.verify_export(self.root)
        finally:
            private.chmod(0o700)

    def test_empty_directories_are_bounded(self) -> None:
        for index in range(12):
            (self.root / f"empty-{index}").mkdir()
        with mock.patch.object(export, "MAX_FILES", 5):
            with self.assertRaisesRegex(export.ExportError, "inventory limit"):
                export.verify_export(self.root)

    def test_payload_replacement_during_hashing_fails(self) -> None:
        original = export._hash_file
        replaced = False

        def replace_after_read(stream: object, size: int) -> str:
            nonlocal replaced
            digest = original(stream, size)
            if not replaced:
                replaced = True
                target = self.root / "lib/libapp.so"
                replacement = self.root / "replacement"
                replacement.write_bytes(b"library")
                replacement.chmod(0o644)
                replacement.replace(target)
            return digest

        with mock.patch.object(export, "_hash_file", side_effect=replace_after_read):
            with self.assertRaises(export.ExportError):
                export.verify_export(self.root)


    def test_parent_rename_cannot_validate_a_different_root_path(self) -> None:
        parent = self.root.parent / "A"
        parent.mkdir()
        relocated = parent / "client"
        self.root.rename(relocated)
        self.root = relocated
        original = export._hash_file
        replaced = False

        def replace_parent(stream: object, size: int) -> str:
            nonlocal replaced
            digest = original(stream, size)
            if not replaced:
                replaced = True
                parent.rename(parent.with_name("B"))
                parent.mkdir()
                (parent / "client").mkdir()
            return digest

        with mock.patch.object(export, "_hash_file", side_effect=replace_parent):
            with self.assertRaisesRegex(export.ExportError, "pathname changed"):
                export.verify_export(self.root)

    @unittest.skipUnless(shutil.which("cc") and shutil.which("readelf"), "ELF toolchain required")
    def test_real_elf_inspection_and_explicit_dependency_closure(self) -> None:
        source = self.root / "library.c"
        source.write_text("int value(void) { return 0; }\n")
        library = self.root / "libsample.so"
        subprocess.run(["cc", "-shared", "-fPIC", "-Wl,-soname,libsample.so", "-o", str(library), str(source)], check=True)
        source.write_text('extern int value(void); int main(void) { return value(); }\n')
        binary = self.root / "client"
        subprocess.run(["cc", "-o", str(binary), str(source), "-L" + str(self.root), "-lsample", "-Wl,-rpath,$ORIGIN/../lib"], check=True)
        with binary.open("rb") as stream:
            client = export.inspect_elf(stream.fileno())
        with library.open("rb") as stream:
            provider = export.inspect_elf(stream.fileno())
        self.assertIn("libsample.so", client["needed"])
        self.assertEqual(provider["soname"], "libsample.so")
        self.assertTrue(client["interpreter"].startswith("/"))
        self.assertIn("libc.so.6", client["required_versions"])
        objects = {"bin/client": client, "lib/libsample.so": provider}
        report = export.elf_dependency_report(objects, entrypoint="bin/client", host_libraries=frozenset({"libc.so.6"}))
        self.assertEqual(report["dependencies"]["bin/client"]["libsample.so"], "lib/libsample.so")
        self.assertFalse(report["runtime_qualified"])
        self.assertFalse(report["symbol_versions_verified"])
        with self.assertRaisesRegex(export.ExportError, "unresolved dependency"):
            export.elf_dependency_report(objects, entrypoint="bin/client", host_libraries=frozenset())
        with self.assertRaisesRegex(export.ExportError, "ambiguous library provider"):
            export.elf_dependency_report(objects, entrypoint="bin/client", host_libraries=frozenset({"libc.so.6", "libsample.so"}))
        provider["machine"] = "other"
        with self.assertRaisesRegex(export.ExportError, "incompatible object ABI"):
            export.elf_dependency_report(objects, entrypoint="bin/client", host_libraries=frozenset({"libc.so.6"}))

    def test_elf_reports_are_bounded_and_timed_out(self) -> None:
        real_popen = subprocess.Popen
        for source, message in [("import sys; sys.stdout.write('x'*4096)", "size limit"),
                                ("import time; time.sleep(10)", "timeout")]:
            def child(*args, **kwargs):
                return real_popen([sys.executable, "-c", source], **kwargs)
            with self.subTest(message=message), mock.patch.object(export.subprocess, "Popen", side_effect=child), \
                 mock.patch.object(export, "MAX_ELF_REPORT_BYTES", 1024):
                with self.assertRaisesRegex(export.ExportError, message):
                    export._readelf(0, timeout=.05)

    def test_elf_inspection_rejects_non_elf_without_executing_it(self) -> None:
        payload = self.root / "not-elf"
        payload.write_text("#!/bin/sh\nexit 0\n")
        with payload.open("rb") as stream, mock.patch.object(export, "_readelf") as inspect:
            with self.assertRaisesRegex(export.ExportError, "ELF magic"):
                export.inspect_elf(stream.fileno())
            inspect.assert_not_called()

    def test_elf_closure_rejects_unmaterialized_alias_and_escaping_search(self) -> None:
        facts = {"class": "ELF64", "endianness": "little", "machine": "test",
                 "needed": [], "soname": "libother.so", "search_paths": [], "interpreter": None}
        with self.assertRaisesRegex(export.ExportError, "alias must be materialized"):
            export.elf_dependency_report({"lib/libsample.so": facts}, entrypoint="lib/libsample.so", host_libraries=frozenset())
        facts["soname"] = None
        facts["search_paths"] = ["$ORIGIN/../../outside"]
        with self.assertRaisesRegex(export.ExportError, "escapes export"):
            export.elf_dependency_report({"bin/client": facts}, entrypoint="bin/client", host_libraries=frozenset())

    def test_elf_extraction_rejects_truncated_or_injected_records(self) -> None:
        payload = self.root / "elf-fixture"
        payload.write_bytes(b"\x7fELF")
        header = "Class: ELF64\nData: 2's complement, little endian\nMachine: fixture\nType: DYN (Shared object file)\n"
        needed = " 0x1 (NEEDED) Shared library: [libc.so.6]\n"
        interpreter = " INTERP 0x0\n [Requesting program interpreter: /lib64/ld-linux-x86-64.so.2]\n"
        version = (" 0x2 (VERNEEDNUM) 1\nVersion needs section '.gnu.version_r' contains 1 entry:\n"
                   " 000000: Version: 1 File: libc.so.6 Cnt: 1\n"
                   " 0x0010: Name: GLIBC_2.34 Flags: none Version: 2\n")
        golden = header + needed + interpreter + version
        with payload.open("rb") as stream, mock.patch.object(export, "_readelf", return_value=golden):
            self.assertEqual(export.inspect_elf(stream.fileno())["required_versions"], {"libc.so.6": ["GLIBC_2.34"]})
        cases = [golden.replace("[libc.so.6]", "[libc.so.6]suffix]"),
                 golden.replace("[libc.so.6]", "[libc.\nso.6]"),
                 golden.replace("[libc.so.6]", "[libc.so.6"),
                 golden.replace(needed, needed + needed),
                 golden.replace("ld-linux-x86-64.so.2]", "ld-linux\n-x86-64.so.2]"),
                 golden.replace(" [Requesting program interpreter: /lib64/ld-linux-x86-64.so.2]\n", ""),
                 golden.replace("Name: GLIBC_2.34", "Name: GLIBC_\n2.34"),
                 golden.replace("Cnt: 1", "Cnt: 2"),
                 golden.replace("contains 1 entry:", "contains 2 entries:"),
                 golden.replace(version, " 0x2 (VERNEEDNUM) 1\n")]
        for text in cases:
            with self.subTest(text=text), payload.open("rb") as stream, \
                 mock.patch.object(export, "_readelf", return_value=text):
                with self.assertRaises(export.ExportError):
                    export.inspect_elf(stream.fileno())

    def test_descriptor_copy_verifies_written_bytes_and_partial_writes(self) -> None:
        source = self.root / "payload"
        source.write_bytes(b"media-bytes" * 1000)
        target = self.root / "copied"
        expected = hashlib.sha256(source.read_bytes()).hexdigest()
        real_write = os.write
        with source.open("rb") as incoming, target.open("x+b") as outgoing, \
             mock.patch.object(export.os, "write", side_effect=lambda fd, data: real_write(fd, data[:17])):
            record = export.copy_payload(incoming.fileno(), outgoing.fileno(), sha256=expected,
                                         size=source.stat().st_size, executable=True)
        self.assertEqual(target.read_bytes(), source.read_bytes())
        self.assertEqual(record["sha256"], expected)
        self.assertEqual(target.stat().st_mode & 0o777, 0o755)

    def test_descriptor_copy_rejects_pointer_bad_hash_and_existing_target(self) -> None:
        for value, expected, occupied in [(export.LFS_HEADER, None, False), (b"actual", "0" * 64, False),
                                           (b"actual", None, True)]:
            with self.subTest(value=value, occupied=occupied):
                source = self.root / "payload"
                source.write_bytes(value)
                target = self.root / "copied"
                target.write_bytes(b"preserve" if occupied else b"")
                with source.open("rb") as incoming, target.open("r+b") as outgoing:
                    with self.assertRaises(export.ExportError):
                        export.copy_payload(incoming.fileno(), outgoing.fileno(),
                                            sha256=expected or hashlib.sha256(value).hexdigest(), size=len(value))
                if occupied:
                    self.assertEqual(target.read_bytes(), b"preserve")

    def test_launcher_moves_and_keeps_state_outside_payload(self) -> None:
        root = self.root / "original export"
        (root / "bin").mkdir(parents=True)
        (root / "share/games/atrinik").mkdir(parents=True)
        binary = root / "bin/atrinik"
        binary.write_text('#!/bin/sh\nprintf "%s\\n" "$PWD" "$ATRINIK_CONFIG_DIR" "$LD_LIBRARY_PATH" "$@"\n')
        binary.chmod(0o755)
        launcher = root / "launch"
        launcher.write_bytes(export.launcher_script())
        launcher.chmod(0o755)
        moved = self.root / "moved export"
        root.rename(moved)
        config = self.root / "external state"
        environment = dict(os.environ, ATRINIK_CONFIG_DIR=str(config))
        result = subprocess.run([str(moved / "launch"), "argument with spaces", "--server", "endpoint"],
                                cwd="/", env=environment, capture_output=True, text=True, check=True)
        self.assertEqual(result.stdout.splitlines(), [str(moved / "share/games/atrinik"), str(config),
                                                     str(moved / "lib"), "argument with spaces", "--server", "endpoint"])
        environment["ATRINIK_CONFIG_DIR"] = str(moved / "state")
        failed = subprocess.run([str(moved / "launch")], env=environment, capture_output=True, text=True)
        self.assertEqual(failed.returncode, 2)
        self.assertFalse((moved / "state").exists())
        if shutil.which("shellcheck"):
            subprocess.run(["shellcheck", str(moved / "launch")], check=True)

    def test_descriptor_copy_rejects_source_mutation_after_successful_read(self) -> None:
        source = self.root / "payload"
        payload = b"a" * 2048
        source.write_bytes(payload)
        target = self.root / "copy"
        original_read = os.pread
        with source.open("rb") as incoming, source.open("r+b") as writer, target.open("x+b") as outgoing:
            def mutate(descriptor, count, offset):
                block = original_read(descriptor, count, offset)
                if descriptor == incoming.fileno() and count == len(payload):
                    os.pwrite(writer.fileno(), b"b", 0)
                return block
            with mock.patch.object(export.os, "pread", side_effect=mutate):
                with self.assertRaisesRegex(export.ExportError, "source bytes changed"):
                    export.copy_payload(incoming.fileno(), outgoing.fileno(),
                                        sha256=hashlib.sha256(payload).hexdigest(), size=len(payload))
        # Copied bytes alone match: the descriptor identity recheck caught drift.
        self.assertEqual(target.read_bytes(), payload)

    def test_descriptor_copy_rejects_staging_mutation_during_readback(self) -> None:
        source = self.root / "payload"
        payload = b"a" * 2048
        source.write_bytes(payload)
        target = self.root / "copy"
        original_read = os.pread
        with source.open("rb") as incoming, target.open("x+b") as outgoing:
            def mutate(descriptor, count, offset):
                block = original_read(descriptor, count, offset)
                if descriptor == outgoing.fileno() and offset == 0:
                    os.pwrite(descriptor, b"b", 0)
                return block
            with mock.patch.object(export.os, "pread", side_effect=mutate):
                with self.assertRaisesRegex(export.ExportError, "staging bytes changed"):
                    export.copy_payload(incoming.fileno(), outgoing.fileno(),
                                        sha256=hashlib.sha256(payload).hexdigest(), size=len(payload))


if __name__ == "__main__":
    unittest.main()
