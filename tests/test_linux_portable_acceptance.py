from __future__ import annotations

import hashlib
import io
import json
from unittest import mock
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import unittest

from atrinik_workspace import linux_export as export
from atrinik_workspace import linux_portable as portable
from atrinik_workspace.model import WorkspaceError


@unittest.skipUnless(sys.platform.startswith("linux"), "Linux portable publication")
class PortablePublicationTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name)
        self.root.chmod(0o700)
        self.output = self.root / "client"
        self.sources = {"atrinik/classic@main": "a" * 40}

    def populate(self, publication):
        publication.add_bytes("bin/atrinik", b"client", executable=True)
        publication.add_bytes("lib/library", b"library")
        publication.manifest["application_libraries"].append("lib/library")
        publication.add_bytes("licenses/NOTICE", b"license", notice=True)

    def test_complete_verified_export_moves_without_source_paths(self):
        with portable.Publication(self.output, self.sources) as publication:
            self.populate(publication)
            staged = publication.path
            result = publication.publish(lambda: None)
        self.assertFalse(staged.exists())
        self.assertEqual(result["files"], 3)
        moved = self.root / "moved folder"
        self.output.rename(moved)
        self.assertEqual(export.verify_export(moved)["sources"], self.sources)

    def test_final_source_guard_failure_does_not_publish_or_remove_staging(self):
        with portable.Publication(self.output, self.sources) as publication:
            self.populate(publication)
            staged = publication.path
            def reject():
                raise export.ExportError("changed source")
            with self.assertRaisesRegex(export.ExportError, "changed source"):
                publication.publish(reject)
        self.assertFalse(self.output.exists())
        self.assertTrue(staged.is_dir())
        export.verify_export(staged)

    def test_concurrent_destination_is_preserved(self):
        with portable.Publication(self.output, self.sources) as publication:
            self.populate(publication)
            def occupy():
                self.output.mkdir()
                (self.output / "keep").write_text("other output")
            with self.assertRaisesRegex(WorkspaceError, "already exists"):
                publication.publish(occupy)
        self.assertEqual((self.output / "keep").read_text(), "other output")

    def test_parent_swap_rejects_publication_into_detached_tree(self):
        parent = self.root / "exports"
        parent.mkdir(mode=0o700)
        with portable.Publication(parent / "client", self.sources) as publication:
            self.populate(publication)
            def swap():
                parent.rename(self.root / "parked")
                parent.mkdir(mode=0o700)
            with self.assertRaises((export.ExportError, FileNotFoundError)):
                publication.publish(swap)
        self.assertFalse((parent / "client").exists())
        self.assertFalse((self.root / "parked/client").exists())

    def test_staging_corruption_during_final_guard_is_rejected(self):
        with portable.Publication(self.output, self.sources) as publication:
            self.populate(publication)
            def corrupt():
                (publication.path / "bin/atrinik").write_bytes(b"wrong!")
            with self.assertRaisesRegex(export.ExportError, "digest mismatch"):
                publication.publish(corrupt)
        self.assertFalse(self.output.exists())

    def test_output_symlink_and_occupied_destination_are_rejected(self):
        self.output.symlink_to(self.root, target_is_directory=True)
        with self.assertRaisesRegex(export.ExportError, "already exists"):
            portable.Publication(self.output, self.sources)
        alias = self.root / "alias"
        alias.symlink_to(self.root, target_is_directory=True)
        with self.assertRaises(OSError):
            portable.Publication(alias / "new", self.sources)

    def test_actual_source_hash_and_special_files_are_rejected(self):
        source = self.root / "payload"
        source.write_bytes(b"actual")
        with portable.Publication(self.output, self.sources) as publication:
            with self.assertRaisesRegex(export.ExportError, "expected payload mismatch"):
                publication.add_path("media/input", source, {"sha256": "f" * 64})
            source.unlink()
            os.mkfifo(source)
            with self.assertRaisesRegex(export.ExportError, "regular file"):
                publication.add_path("media/input", source)
        self.assertFalse(self.output.exists())

    def test_group_writable_ancestor_is_rejected(self):
        ancestor = self.root / "shared"
        parent = ancestor / "mine"
        parent.mkdir(parents=True, mode=0o700)
        ancestor.chmod(0o775)
        with self.assertRaisesRegex(export.ExportError, "unsafe ancestor"):
            portable.Publication(parent / "client", self.sources)

    def test_hexadecimal_icu_symbol_size_is_retained_and_missing_rows_fail(self):
        table = """Symbol table '.dynsym' contains 2 entries:
   Num:    Value          Size Type    Bind   Vis      Ndx Name
     0: 0000000000000000     0 NOTYPE  LOCAL  DEFAULT  UND
     1: 0000000000002000 0x1f92d30 OBJECT GLOBAL DEFAULT 11 icudt72_dat
"""
        self.assertEqual(export._elf_dynamic_symbols(table)["defined"], ["icudt72_dat"])
        with self.assertRaisesRegex(export.ExportError, "incomplete dynamic"):
            export._elf_dynamic_symbols(table.replace("contains 2", "contains 3"))

    def test_acceptance_driver_refuses_display_or_audio_endpoint_before_execution(self):
        driver = Path(__file__).resolve().parents[1] / "scripts/linux_portable_acceptance.py"
        for variable in ("DISPLAY", "WAYLAND_DISPLAY", "PULSE_SERVER", "PIPEWIRE_REMOTE"):
            environment = dict(os.environ)
            environment[variable] = "forbidden-endpoint"
            code = ("import runpy; value=runpy.run_path(" + repr(str(driver)) + "); "
                    "value['require_headless']()")
            result = subprocess.run([sys.executable, "-c", code], env=environment,
                                    text=True, capture_output=True, timeout=10)
            with self.subTest(variable=variable):
                self.assertNotEqual(result.returncode, 0)
                self.assertIn("must not receive display or audio endpoints", result.stderr)

    def test_acceptance_detects_application_library_loaded_outside_export(self):
        import runpy
        driver = Path(__file__).resolve().parents[1] / "scripts/linux_portable_acceptance.py"
        namespace = runpy.run_path(str(driver))
        (self.root / "lib").mkdir()
        (self.root / "lib/libexample.so.1").write_bytes(b"placeholder")
        mapping = "7f000-7f100 r-xp 0000 00:00 1 /usr/lib/libexample.so.1\n"
        with mock.patch.object(Path, "read_text", return_value=mapping):
            with self.assertRaisesRegex(RuntimeError, "provider escaped moved export"):
                namespace["loaded_application_paths"](self.root)

    @unittest.skipUnless(shutil.which("cc"), "native compiler")
    def test_real_loader_rejects_omitted_application_library_even_without_manifest_entry(self):
        import runpy
        namespace = runpy.run_path(str(Path(__file__).resolve().parents[1] / "scripts/linux_portable_acceptance.py"))
        source = self.root / "consumer.c"
        source.write_text("extern const char *zlibVersion(void); int main(void) { return !zlibVersion(); }\n")
        binary = self.root / "consumer"
        subprocess.run(["cc", str(source), "-o", str(binary), "-lz"], check=True, capture_output=True)
        environment = dict(os.environ)
        environment.pop("LD_LIBRARY_PATH", None)
        listing = subprocess.check_output(["/lib64/ld-linux-x86-64.so.2", "--list", str(binary)],
                                          text=True, env=environment)
        self.assertIn("libz.so.1", listing)
        with self.assertRaisesRegex(RuntimeError, "external application library"):
            namespace["verify_loader_listing"](self.root, listing)
        trace = "123: calling init: /usr/lib/x86_64-linux-gnu/libz.so.1\n"
        with self.assertRaisesRegex(RuntimeError, "external application library"):
            namespace["verify_loader_trace"](self.root, trace)

    def test_complete_source_notice_inventory_rejects_missing_corrupt_and_extra_files(self):
        source = self.root / "producer"
        source.mkdir()
        inputs = {"sources/mixer.tar.gz": b"mixer corresponding source",
                  "build.py": b"build recipe", "notices/SDL_mixer.txt": b"required attribution"}
        records = {}
        for name, payload in inputs.items():
            path = source / name
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_bytes(payload)
            records[name] = {"sha256": hashlib.sha256(payload).hexdigest(),
                             "size": len(payload), "executable": False}
        expected = hashlib.sha256(json.dumps(records, sort_keys=True, separators=(",", ":")).encode()).hexdigest()
        self.assertEqual(portable.verified_tree_inventory(source, expected), records)
        for name, payload in inputs.items():
            for mutation in ("missing", "corrupt", "mode"):
                path = source / name
                with self.subTest(name=name, mutation=mutation):
                    if mutation == "missing":
                        path.unlink()
                    elif mutation == "corrupt":
                        path.write_bytes(b"invalid notice/source")
                    else:
                        path.chmod(0o755)
                    with self.assertRaisesRegex(export.ExportError, "inventory mismatch"):
                        portable.verified_tree_inventory(source, expected)
                    path.write_bytes(payload)
                    path.chmod(0o644)
        (source / "extra").write_bytes(b"unrecorded")
        with self.assertRaisesRegex(export.ExportError, "inventory mismatch"):
            portable.verified_tree_inventory(source, expected)

    def test_source_deletion_between_inventory_and_copy_is_rejected(self):
        source = self.root / "source"
        source.mkdir()
        with portable.Publication(self.output, self.sources) as publication:
            with self.assertRaisesRegex(export.ExportError, "inventory differs"):
                portable.add_regular_tree(publication, source, "sources", expected={
                    "missing": {"sha256": "a" * 64, "size": 10, "executable": False}})

    def test_export_rejects_nonpublishable_sound_before_building(self):
        for mode in ("source", "local-playtest"):
            with self.subTest(mode=mode):
                workspace = mock.Mock()
                workspace._load_profile.return_value = {"stack": "classic", "sound_mode": mode}
                with mock.patch.object(portable, "installed_metadata", return_value=({}, {})):
                    with self.assertRaisesRegex(WorkspaceError, "verified released sound"):
                        portable.export_client(workspace, "classic", self.output)
                workspace._resolved_profile_operation.assert_not_called()
                workspace._build_resolved.assert_not_called()

    def test_cli_routes_real_build_chatter_and_errors_to_stderr(self):
        from atrinik_workspace import cli
        from atrinik_workspace.workspace import run
        for fail in (False, True):
            def noisy_export(*args):
                run([sys.executable, "-c", "print('build diagnostic')"])
                if fail:
                    raise WorkspaceError("build failed")
                return {"path": str(self.output)}
            with self.subTest(fail=fail), tempfile.TemporaryFile(mode="w+") as errors:
                output = io.StringIO()
                with mock.patch.object(cli, "Workspace"), \
                     mock.patch.object(portable, "export_client", side_effect=noisy_export), \
                     mock.patch.object(sys, "stdout", output), \
                     mock.patch.object(sys, "stderr", errors):
                    status = cli.main(["linux", "export", "--output", str(self.output)])
                self.assertEqual(status, int(fail))
                if fail:
                    self.assertEqual(output.getvalue(), "")
                else:
                    self.assertEqual(json.loads(output.getvalue()), {"path": str(self.output)})
                errors.seek(0)
                self.assertIn("build diagnostic", errors.read())

    def test_unloaded_plugin_cannot_satisfy_client_symbol(self):
        objects = {
            "bin/client": {"needed": [], "symbols": {"defined": ["main"], "required": ["plugin_only"]}},
            "lib/plugin.so": {"needed": [], "symbols": {"defined": ["plugin_only"], "required": []}},
        }
        with self.assertRaisesRegex(export.ExportError, "unresolved symbols in bin/client"):
            portable.symbol_closure(objects, entrypoint="bin/client")

    @unittest.skipUnless(shutil.which("cc") and shutil.which("readelf"), "native ELF tools")
    def test_portable_executable_rejects_custom_or_missing_loader(self):
        source = self.root / "main.c"
        source.write_text("int main(void) { return 0; }\n")
        for name, options in (("normal", []), ("custom", ["-Wl,--dynamic-linker=/tmp/private-loader"]),
                              ("missing", ["-shared", "-fPIC"])):
            target = self.root / name
            subprocess.run(["cc", str(source), "-o", str(target), *options], check=True, capture_output=True)
            with target.open("rb") as stream:
                if name == "normal":
                    self.assertEqual(portable.portable_executable(stream.fileno())["interpreter"],
                                     "/lib64/ld-linux-x86-64.so.2")
                else:
                    with self.assertRaisesRegex(export.ExportError, "supported host loader"):
                        portable.portable_executable(stream.fileno())
        target = self.root / "normal"
        target.chmod(0o644)
        with target.open("rb") as stream, self.assertRaisesRegex(export.ExportError, "executable mode"):
            portable.portable_executable(stream.fileno())

    @unittest.skipUnless(shutil.which("cc") and shutil.which("readelf"), "native ELF tools")
    def test_real_versioned_symbol_definitions_and_missing_symbol(self):
        source = self.root / "sample.c"
        version = self.root / "version.map"
        library = self.root / "libsample.so"
        source.write_text("int sample(void) { return 7; }\n")
        version.write_text("SAMPLE_1 { global: sample; local: *; };\n")
        subprocess.run(["cc", "-shared", "-fPIC", str(source), "-o", str(library),
                        "-Wl,--version-script=" + str(version)], check=True, capture_output=True)
        with library.open("rb") as stream:
            facts = export.inspect_elf(stream.fileno())
        self.assertIn("sample@SAMPLE_1", facts["symbols"]["defined"])
        self.assertIn("sample", facts["symbols"]["defined"])
        consumer = {"needed": ["lib"], "symbols": {"defined": ["main"], "required": ["sample@SAMPLE_1"]}}
        self.assertTrue(portable.symbol_closure({"lib": facts, "client": consumer}, entrypoint="client")["symbol_names_verified"])
        consumer["symbols"]["required"] = ["sample@SAMPLE_2"]
        with self.assertRaisesRegex(export.ExportError, "unresolved symbols"):
            portable.symbol_closure({"lib": facts, "client": consumer}, entrypoint="client")
