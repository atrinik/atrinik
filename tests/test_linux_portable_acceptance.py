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
import tarfile
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

    def audio_archive(self, *, duplicate=False, linked=False):
        payload = b"Original attribution and license text\n"
        buffer = io.BytesIO()
        name = "library-1/COPYING"
        with tarfile.open(fileobj=buffer, mode="w:gz") as archive:
            member = tarfile.TarInfo(name)
            member.size = len(payload)
            if linked:
                member.type = tarfile.SYMTYPE
                member.linkname = "/outside"
            archive.addfile(member, io.BytesIO(payload))
            if duplicate:
                archive.addfile(member, io.BytesIO(payload))
        data = buffer.getvalue()
        digest = hashlib.sha256(data).hexdigest()
        source = self.root / "sources"
        source.mkdir()
        (source / (digest + ".tar.gz")).write_bytes(data)
        record = {"library": {"sha256": digest, "members": {
            name: {"sha256": hashlib.sha256(payload).hexdigest(), "size": len(payload)}}}}
        return record, payload

    def test_audio_notices_preserve_verified_text_and_archive_provenance(self):
        record, payload = self.audio_archive()
        with mock.patch.object(portable, "AUDIO_NOTICES", record), portable.Publication(self.output, self.sources) as publication:
            evidence = portable.add_audio_notices(publication, self.root)
            self.assertEqual((publication.path / "licenses/audio/library/COPYING").read_bytes(), payload)
            self.assertEqual(evidence[0]["archive_sha256"], record["library"]["sha256"])
            self.assertIn("licenses/audio/library/COPYING", publication.manifest["notices"])

    def test_audio_notices_reject_duplicate_members_and_links(self):
        for option in ("duplicate", "linked"):
            with self.subTest(option=option):
                if (self.root / "sources").exists():
                    shutil.rmtree(self.root / "sources")
                record, _ = self.audio_archive(**{option: True})
                with mock.patch.object(portable, "AUDIO_NOTICES", record), portable.Publication(self.output, self.sources) as publication:
                    with self.assertRaisesRegex(export.ExportError, "audio notice member"):
                        portable.add_audio_notices(publication, self.root)

    def test_audio_notices_reject_wrong_archive_member_hash_or_missing_member(self):
        record, _ = self.audio_archive()
        for mutation in ("archive", "hash", "missing"):
            altered = json.loads(json.dumps(record))
            if mutation == "archive":
                digest = altered["library"]["sha256"]
                source = self.root / "sources" / (digest + ".tar.gz")
                original = source.read_bytes()
                source.write_bytes(b"changed")
            elif mutation == "hash":
                altered["library"]["members"]["library-1/COPYING"]["sha256"] = "0" * 64
            else:
                altered["library"]["members"]["library-1/MISSING"] = altered["library"]["members"].pop("library-1/COPYING")
            with mock.patch.object(portable, "AUDIO_NOTICES", altered), portable.Publication(self.output, self.sources) as publication:
                with self.assertRaisesRegex(export.ExportError, "portable-legal"):
                    portable.add_audio_notices(publication, self.root)
            if mutation == "archive":
                source.write_bytes(original)

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
    def test_real_launcher_helpers_are_separate_from_client_loader_trace(self):
        import runpy
        namespace = runpy.run_path(str(Path(__file__).resolve().parents[1] / "scripts/linux_portable_acceptance.py"))
        for path in ("bin", "lib", "share/games/atrinik"):
            (self.output / path).mkdir(parents=True)
        source = self.root / "client.c"
        source.write_text('#include <stdio.h>\nint main(void) { puts("client help"); return 0; }\n')
        subprocess.run(["cc", str(source), "-o", str(self.output / "bin/atrinik")],
                       check=True, capture_output=True)
        launcher = self.output / "atrinik"
        launcher.write_bytes(export.launcher_script())
        launcher.chmod(0o755)
        config = self.root / "private config"
        with mock.patch.dict(os.environ, {"ATRINIK_CONFIG_DIR": str(config)}):
            paths = namespace["verify_client_launch"](self.output, self.root)
        self.assertTrue(config.is_dir())
        self.assertTrue(paths)
        self.assertFalse(any("libselinux" in path for path in paths))
        self.assertIn("initialize program:", (self.root / "client-loader.log").read_text())

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


    def derivative_fixture(self):
        original = b"/usr/lib/x86_64-linux-gnu/pulseaudio\0"
        data = b"private fixture prefix" + original + b"unmodified tail"
        replacement = b"$ORIGIN\0".ljust(len(original), b"\0")
        result = data[:22] + replacement + data[22 + len(original):]
        recipe = dict(portable.PULSE_RECIPE, offset=22, size=len(data),
                      input_sha256=hashlib.sha256(data).hexdigest(),
                      output_sha256=hashlib.sha256(result).hexdigest())
        return data, result, recipe

    def test_fixed_derivative_preserves_other_bytes_and_reproduces_from_shipped_recipe(self):
        from contextlib import ExitStack
        data, expected, recipe = self.derivative_fixture()
        source = self.root / "source-library"
        source.write_bytes(data)
        destination = self.root / "reproduced-library"
        with mock.patch.object(portable, "PULSE_RECIPE", recipe):
            self.assertEqual(portable.pulse_derivative(data), expected)
            with ExitStack() as stack:
                stream = stack.enter_context(source.open("rb"))
                descriptor, record, provenance = portable.provider_payload(
                    stack, Path(recipe["provider_path"]), stream.fileno(), portable.byte_record(stream.fileno()))
                self.assertNotEqual(descriptor, stream.fileno())
                self.assertEqual(os.pread(descriptor, 1000, 0), expected)
                self.assertEqual(record["sha256"], recipe["output_sha256"])
                self.assertEqual(provenance["input_sha256"], recipe["input_sha256"])
            script = self.root / "reproduce.py"
            script.write_bytes(portable.derivative_recipe_script())
            subprocess.run([sys.executable, str(script), str(source), str(destination)], check=True)
            self.assertEqual(destination.read_bytes(), expected)
            retry = subprocess.run([sys.executable, str(script), str(source), str(destination)], capture_output=True)
            self.assertNotEqual(retry.returncode, 0)
        self.assertEqual(source.read_bytes(), data)

    def test_fixed_derivative_rejects_hash_span_size_and_output_changes(self):
        data, expected, recipe = self.derivative_fixture()
        for change, value, message in (
            ("input_sha256", "f" * 64, "input differs"),
            ("size", len(data) + 1, "input differs"),
            ("offset", 0, "string span differs"),
            ("output_sha256", "f" * 64, "output digest differs"),
            ("replacement", "wrong", "output digest differs"),
        ):
            with self.subTest(change=change), mock.patch.object(portable, "PULSE_RECIPE", {**recipe, change: value}):
                with self.assertRaisesRegex(export.ExportError, message):
                    portable.pulse_derivative(data)
        with mock.patch.object(portable, "PULSE_RECIPE", recipe):
            with self.assertRaisesRegex(export.ExportError, "input differs"):
                portable.pulse_derivative(data[:-1] + b"!")

    def test_derivative_requires_exact_provider_coordinate_and_original_record(self):
        from contextlib import ExitStack
        data, expected, recipe = self.derivative_fixture()
        source = self.root / "library"
        source.write_bytes(data)
        with mock.patch.object(portable, "PULSE_RECIPE", recipe), source.open("rb") as stream, ExitStack() as stack:
            record = portable.byte_record(stream.fileno())
            descriptor, same_record, provenance = portable.provider_payload(stack, source, stream.fileno(), record)
            self.assertEqual(descriptor, stream.fileno())
            self.assertEqual(same_record, record)
            self.assertIsNone(provenance)
            for key, value in (("IMAGE", "another-image"), ("PLATFORM_MANIFEST", "sha256:" + "f" * 64)):
                with mock.patch.object(portable, key, value):
                    with self.assertRaisesRegex(export.ExportError, "producer coordinate differs"):
                        portable.provider_payload(stack, Path(recipe["provider_path"]), stream.fileno(), record)
            with self.assertRaisesRegex(export.ExportError, "record differs"):
                portable.provider_payload(stack, Path(recipe["provider_path"]), stream.fileno(), {**record, "sha256": "f" * 64})

    def acceptance_namespace(self):
        import runpy
        return runpy.run_path(str(Path(__file__).resolve().parents[1] / "scripts/linux_portable_acceptance.py"))

    def test_failed_export_capture_preserves_bounded_logs_and_exit_status(self):
        ns = self.acceptance_namespace()
        command = [sys.executable, "-c", "import sys; print('provider rejected'); print('strict ELF failure', file=sys.stderr); sys.exit(23)"]
        with self.assertRaises(subprocess.CalledProcessError) as error:
            ns["capture_export"](command, self.root, timeout=10)
        self.assertEqual(error.exception.returncode, 23)
        self.assertIn("provider rejected", (self.root / "export.stdout.log").read_text())
        self.assertIn("strict ELF failure", (self.root / "export.stderr.log").read_text())
        self.assertEqual(json.loads((self.root / "export-process.json").read_text())["returncode"], 23)
        self.assertFalse((self.root / "export.json").exists())

    def test_export_capture_timeout_retains_failure_and_bounded_output(self):
        ns = self.acceptance_namespace()
        with self.assertRaises(subprocess.TimeoutExpired):
            ns["capture_export"]([sys.executable, "-c", "import time; print('before timeout', flush=True); time.sleep(30)"], self.root, timeout=0.2)
        self.assertIn("before timeout", (self.root / "export.stdout.log").read_text())
        self.assertTrue(json.loads((self.root / "export-process.json").read_text())["timed_out"])

    def test_export_capture_drains_large_logs_without_unbounded_retention(self):
        ns = self.acceptance_namespace()
        limit = ns["MAX_CAPTURE_BYTES"]
        result = ns["capture_export"]([sys.executable, "-c", "import sys; sys.stderr.write('x' * " + str(limit * 2) + "); print('{}')"], self.root, timeout=10)
        self.assertEqual(json.loads(result), {})
        self.assertEqual((self.root / "export.stderr.log").stat().st_size, limit)
        report = json.loads((self.root / "export-process.json").read_text())
        self.assertEqual(report["observed_bytes"]["stderr"], limit * 2)

    def test_build_failure_keeps_validated_inputs_and_does_not_mask_root_error(self):
        ns = self.acceptance_namespace()
        globals_ = ns["build"].__globals__
        failure = subprocess.CalledProcessError(19, ["./atrinik", "linux", "export"])
        def fake_run(arguments, **kwargs):
            if arguments[0] == "git" and "rev-parse" in arguments:
                return ns["SOURCE_COMMITS"].get(arguments[2], "a" * 40) + "\n" if "-C" in arguments else "a" * 40 + "\n"
            return ""
        with mock.patch.dict(globals_, {"require_headless": lambda: None, "run": fake_run,
                              "capture_export": mock.Mock(side_effect=failure),
                              "collect_build_evidence": mock.Mock(side_effect=RuntimeError("collector failed"))}), \
             mock.patch.object(os, "sched_setaffinity"), \
             mock.patch.object(portable, "installed_metadata", return_value=({}, {})):
            with self.assertRaises(subprocess.CalledProcessError) as caught:
                ns["build"](self.output, self.root)
        self.assertIs(caught.exception, failure)
        self.assertEqual(json.loads((self.root / "inputs.json").read_text())["sources"], ns["SOURCE_COMMITS"])
        self.assertEqual(json.loads((self.root / "failure.json").read_text())["returncode"], 19)
        self.assertIn("collector failed", (self.root / "collector-failure.json").read_text())
        self.assertFalse((self.root / "export.json").exists())
        self.assertFalse((self.root / "client.tar").exists())


    @unittest.skipUnless(shutil.which("cc") and shutil.which("readelf"), "native ELF tools")
    def test_other_absolute_runpath_provider_remains_rejected(self):
        from contextlib import ExitStack
        source = self.root / "provider.c"
        library = self.root / "libother.so"
        source.write_text("int other(void) { return 0; }\n")
        subprocess.run(["cc", "-shared", "-fPIC", str(source), "-o", str(library),
                        "-Wl,-rpath,/usr/lib/x86_64-linux-gnu/pulseaudio"], check=True, capture_output=True)
        with library.open("rb") as stream, ExitStack() as stack:
            descriptor, _, derivative = portable.provider_payload(stack, library, stream.fileno(), portable.byte_record(stream.fileno()))
            self.assertIsNone(derivative)
            with self.assertRaises(export.ExportError):
                export.inspect_elf(descriptor)

    def test_derivative_rejects_source_mutation_during_private_copy(self):
        from contextlib import ExitStack
        data, _, recipe = self.derivative_fixture()
        source = self.root / "source"
        source.write_bytes(data)
        original_read = os.pread
        def mutate(descriptor, size, offset):
            result = original_read(descriptor, size, offset)
            source.write_bytes(data + b"changed")
            return result
        with source.open("rb") as stream, ExitStack() as stack, mock.patch.object(portable, "PULSE_RECIPE", recipe):
            record = portable.byte_record(stream.fileno())
            with mock.patch.object(os, "pread", side_effect=mutate):
                with self.assertRaisesRegex(export.ExportError, "changed during read"):
                    portable.provider_payload(stack, Path(recipe["provider_path"]), stream.fileno(), record)

    def test_derivative_handles_short_writes_and_rejects_failed_write(self):
        from contextlib import ExitStack
        data, expected, recipe = self.derivative_fixture()
        source = self.root / "source"
        source.write_bytes(data)
        original_write = os.write
        with source.open("rb") as stream, mock.patch.object(portable, "PULSE_RECIPE", recipe):
            record = portable.byte_record(stream.fileno())
            with ExitStack() as stack, mock.patch.object(os, "write", side_effect=lambda fd, data: original_write(fd, data[:3])):
                descriptor, _, _ = portable.provider_payload(stack, Path(recipe["provider_path"]), stream.fileno(), record)
                self.assertEqual(os.pread(descriptor, 1000, 0), expected)
            with ExitStack() as stack, mock.patch.object(os, "write", return_value=0):
                with self.assertRaisesRegex(export.ExportError, "incomplete private copy"):
                    portable.provider_payload(stack, Path(recipe["provider_path"]), stream.fileno(), record)

    def test_compiler_evidence_collects_only_selected_profile_and_rejects_links(self):
        ns = self.acceptance_namespace()
        previous = Path.cwd()
        os.chdir(self.root)
        try:
            selected = Path("workspace/build/profiles/selected-123/build/client")
            selected.mkdir(parents=True)
            (selected / "CMakeCache.txt").write_text("compiler input")
            (selected / "compile_commands.json").write_text("[]")
            foreign = Path("workspace/build/profiles/foreign-123/build/client")
            foreign.mkdir(parents=True)
            (foreign / "CMakeCache.txt").write_text("foreign must not be copied")
            ns["collect_build_evidence"]("selected", self.root)
            result = json.loads((self.root / "compiler-evidence.json").read_text())
            self.assertEqual(len(result), 1)
            self.assertEqual((self.root / "0-CMakeCache.txt").read_text(), "compiler input")
            self.assertIsNone(result[0]["client"])
            (selected / "compile_commands.json").unlink()
            (selected / "compile_commands.json").symlink_to((foreign / "CMakeCache.txt").resolve())
            with self.assertRaises(OSError):
                ns["collect_build_evidence"]("selected", self.root)
        finally:
            os.chdir(previous)

    def test_occupied_capture_logs_refuse_before_starting_process(self):
        ns = self.acceptance_namespace()
        (self.root / "export.stdout.log").write_text("existing evidence")
        with mock.patch.object(subprocess, "Popen") as spawn:
            with self.assertRaises(FileExistsError):
                ns["capture_export"]([sys.executable, "-c", "pass"], self.root)
        spawn.assert_not_called()
        self.assertEqual((self.root / "export.stdout.log").read_text(), "existing evidence")


    def compiled_runtime(self):
        source = self.root / "provider.c"
        source.write_text("int sample(void) { return 7; }\n")
        versions = self.root / "versions.map"
        versions.write_text("SAMPLE_1 { global: sample; local: *; };\n")
        library = self.root / "provider.so"
        subprocess.run(["cc", "-shared", "-fPIC", "-nostdlib", str(source), "-o", str(library),
                        "-Wl,-soname,libfixture.so.1", "-Wl,--version-script=" + str(versions)],
                       check=True, capture_output=True)
        client_source = self.root / "client.c"
        client_source.write_text("extern int sample(void); int main(void) { return sample(); }\n")
        binary = self.root / "client-binary"
        subprocess.run(["cc", "-nostdlib", str(client_source), str(library), "-o", str(binary),
                        "-Wl,-e,main", "-Wl,--dynamic-linker=/lib64/ld-linux-x86-64.so.2"],
                       check=True, capture_output=True)
        documents = {"runtime-abi.json": {"objects": [{
            "path": str(library), "sha256": hashlib.sha256(library.read_bytes()).hexdigest(),
            "needed": [], "dlopen": [{"feature": "fixture", "soname": ["libfixture.so.1"]}],
            "dlopen_providers": {"fixture": [str(library)]},
            "required_providers": {"sample@SAMPLE_1": {"provider": "libfixture.so.1"}},
        }]}, "contract.json": {"runtime": {"unsupported_dlopen_features": []}}}
        return library, binary, documents

    @unittest.skipUnless(shutil.which("cc") and shutil.which("readelf"), "native ELF tools")
    def test_actual_runtime_elf_closure_is_copied_and_verified_after_relocation(self):
        library, binary, documents = self.compiled_runtime()
        with portable.Publication(self.output, self.sources) as publication:
            report = portable.add_runtime(publication, documents, binary)
            publication.add_bytes("licenses/NOTICE", b"fixture license", notice=True)
            publication.publish(lambda: None)
        moved = self.root / "relocated runtime"
        self.output.rename(moved)
        library.unlink()
        binary.unlink()
        result = export.verify_export(moved)
        self.assertEqual(len(result["files"]), 3)
        self.assertTrue(report["runtime_payload_verified"])
        self.assertTrue(report["symbols"]["symbol_names_verified"])
        self.assertEqual(report["dynamic_sonames"][0]["provider"], "lib/libfixture.so.1")
        self.assertFalse(report["hardware_gameplay_verified"])
        self.assertFalse(report["audible_playback_verified"])
        self.assertEqual(report["derivatives"], [])

    @unittest.skipUnless(shutil.which("cc") and shutil.which("readelf"), "native ELF tools")
    def test_actual_runtime_rejects_provider_hash_dependency_dynamic_and_symbol_drift(self):
        import copy
        library, binary, original = self.compiled_runtime()
        changes = (
            ("hash", "provider hash mismatch"), ("needed", "dependency metadata differs"),
            ("dynamic", "unresolved dynamic SONAME"), ("symbol", "required symbol absent"),
            ("duplicate", "ambiguous provider SONAME"),
        )
        for label, message in changes:
            documents = copy.deepcopy(original)
            row = documents["runtime-abi.json"]["objects"][0]
            if label == "hash":
                row["sha256"] = "f" * 64
            elif label == "needed":
                row["needed"] = ["libmissing.so.1"]
            elif label == "dynamic":
                row["dlopen"][0]["soname"] = ["libmissing.so.1"]
            elif label == "symbol":
                row["required_providers"] = {"missing@SAMPLE_1": {"provider": "libfixture.so.1"}}
            else:
                documents["runtime-abi.json"]["objects"].append(copy.deepcopy(row))
            with self.subTest(label=label), portable.Publication(self.root / label, self.sources) as publication:
                with self.assertRaisesRegex(export.ExportError, message):
                    portable.add_runtime(publication, documents, binary)
            self.assertFalse((self.root / label).exists())

    @unittest.skipUnless(shutil.which("cc") and shutil.which("readelf"), "native ELF tools")
    def test_declared_unsupported_dynamic_feature_is_excluded_explicitly(self):
        library, binary, documents = self.compiled_runtime()
        documents["contract.json"]["runtime"]["unsupported_dlopen_features"] = [
            {"object": str(library), "feature": "fixture", "soname": ["libfixture.so.1"]}]
        with portable.Publication(self.output, self.sources) as publication:
            report = portable.add_runtime(publication, documents, binary)
        self.assertEqual(report["dynamic_sonames"], [])
