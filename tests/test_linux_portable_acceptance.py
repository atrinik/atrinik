from __future__ import annotations

from contextlib import ExitStack, chdir, nullcontext

import hashlib
import io
import json
from unittest import mock
import os
from pathlib import Path
import shutil
import shlex
import subprocess
import stat
import sys
import tarfile
import tempfile
import unittest

from atrinik_workspace import linux_export as export
from atrinik_workspace import linux_portable as portable
from atrinik_workspace.model import WorkspaceError


def fixture_build_tools(root: Path) -> dict:
    """Copy available host tools, never select or authenticate producer tools."""
    root.mkdir(parents=True)
    sources = {}
    for role, candidates in {"cmake": ("cmake",), "ninja": ("ninja",),
                             "pkg_config": ("pkgconf", "pkg-config")}.items():
        selected = next((path for name in candidates if (path := shutil.which(name))), None)
        if selected is None:
            raise AssertionError("missing host fixture tool: " + role)
        sources[role] = Path(selected).resolve(strict=True)
    # Hosted CMake may be a launcher or live outside /usr. Ask the available
    # command for its actual binary and module root without configuring a project.
    probe = root / "probe.cmake"
    probe.write_text('message("FIXTURE_COMMAND=${CMAKE_COMMAND}")\n'
                     'message("FIXTURE_ROOT=${CMAKE_ROOT}")\n')
    result = subprocess.run([str(sources["cmake"]), "-P", str(probe)], check=True,
                            capture_output=True, text=True, timeout=15)
    facts = {line.split("=", 1)[0]: line.split("=", 1)[1]
             for line in result.stderr.splitlines() if line.startswith(("FIXTURE_COMMAND=", "FIXTURE_ROOT="))}
    sources["cmake"] = Path(facts["FIXTURE_COMMAND"]).resolve(strict=True)
    modules = Path(facts["FIXTURE_ROOT"]).resolve(strict=True)
    (root / "share").mkdir()
    (root / "share" / modules.name).symlink_to(modules, target_is_directory=True)
    # CMake distributions may carry runtime libraries beside their bin directory.
    libraries = sources["cmake"].parent.parent / "lib"
    if libraries.is_dir(): (root / "lib").symlink_to(libraries, target_is_directory=True)
    binary = root / "bin"
    binary.mkdir()
    records = {}
    for role, source in sources.items():
        target = binary / role
        shutil.copyfile(source, target)
        target.chmod(0o700)
        records[role] = {"path": str(target), "resolved_path": str(target),
                         "sha256": hashlib.sha256(target.read_bytes()).hexdigest(),
                         "size": target.stat().st_size, "executable": True,
                         "version": subprocess.run([str(target), "--version"], check=True,
                            capture_output=True, text=True, timeout=15,
                            env={"PATH": os.defpath, "LC_ALL": "C"}).stdout}
    return {"schema_version": 1, "tools": records}


def fixture_tool_path_protected(info):
    """Test-only private-copy policy; production remains root-owned and no-follow."""
    return info.st_uid in {0, os.geteuid()} and (
        not info.st_mode & (stat.S_IWGRP | stat.S_IWOTH)
        or stat.S_ISDIR(info.st_mode) and info.st_uid == 0 and info.st_mode & stat.S_ISVTX)


@unittest.skipUnless(sys.platform.startswith("linux"), "Linux copied build-tool fixtures")
class PortableBuildToolTests(unittest.TestCase):
    def setUp(self):
        temporary = tempfile.TemporaryDirectory()
        self.addCleanup(temporary.cleanup)
        self.root = Path(temporary.name)
        self.root_only_policy = portable.PortableBuildContext._build_tool_path_protected
        # Only fixture admission permits our private copied tools/ancestry and
        # the test runner's root-owned sticky temporary anchor. Production is root-only.
        patcher = mock.patch.object(portable.PortableBuildContext, "_build_tool_path_protected",
                                    side_effect=fixture_tool_path_protected)
        patcher.start()
        self.addCleanup(patcher.stop)

    def context(self, *, capture=True):
        installation = self.root / str(len(list(self.root.iterdir())))
        expected = fixture_build_tools(installation)
        root = installation / "bin"
        paths = {name: record["path"] for name, record in expected["tools"].items()}
        with mock.patch.dict(export.PORTABLE_BUILD_TOOL_PATHS, paths, clear=True):
            context = portable.PortableBuildContext({}, {}, {}, {}, build_tools=expected)
        if capture:
            context.build_tools = context._capture_build_tools()
            context.host_tools = {"shader_python": context._capture_generator_python()}
        return context, root

    def test_actual_copied_tools_preserve_complete_producer_versions(self):
        context, root = self.context()
        context.verify_host_tools()
        for name, receipt in context.build_tools.items():
            self.assertEqual(receipt["sha256"], hashlib.sha256((root / name).read_bytes()).hexdigest())
            self.assertEqual(receipt["version"], context.producer_build_tools[name]["version"])
            self.assertTrue(receipt["version"].endswith("\n"))
        binary = root / "build"
        binary.mkdir()
        (binary / "build.ninja").write_text("rule fixture\n  command = /bin/echo fixture\nbuild atrinik: fixture\n")
        self.assertEqual(context._ninja(binary), "/bin/echo fixture\n")

    def test_hosted_prefix_tools_need_no_fixed_producer_paths(self):
        available = fixture_build_tools(self.root / "available-host-prefix")
        commands = {"cmake": available["tools"]["cmake"]["path"],
                    "ninja": available["tools"]["ninja"]["path"],
                    "pkgconf": available["tools"]["pkg_config"]["path"]}
        production_paths = dict(export.PORTABLE_BUILD_TOOL_PATHS)
        with mock.patch.object(shutil, "which", side_effect=lambda name: commands.get(name)):
            copied = fixture_build_tools(self.root / "owned-copy")
        paths = {name: record["path"] for name, record in copied["tools"].items()}
        with mock.patch.dict(export.PORTABLE_BUILD_TOOL_PATHS, paths, clear=True):
            context = portable.PortableBuildContext({}, {}, {}, {}, build_tools=copied)
        context.build_tools = context._capture_build_tools()
        context.host_tools = {"shader_python": context._capture_generator_python()}
        context.verify_host_tools()
        self.assertEqual(export.PORTABLE_BUILD_TOOL_PATHS, production_paths)
        self.assertTrue(all(path.startswith(str(self.root / "owned-copy")) for path in paths.values()))

    def test_production_policy_rejects_consumer_owned_execution_paths_before_probes(self):
        context, _root = self.context(capture=False)
        with mock.patch.object(portable.PortableBuildContext, "_build_tool_path_protected",
                               side_effect=self.root_only_policy), mock.patch.object(export, "_tool_report") as report, \
             self.assertRaisesRegex(ValueError, "build tool ancestor"):
            context._capture_build_tools()
        report.assert_not_called()

    def test_writable_ancestor_and_intermediate_alias_are_rejected_before_probes(self):
        for mutation in ("writable", "alias"):
            context, root = self.context(capture=False)
            if mutation == "writable": root.chmod(0o777)
            else:
                relocated = root.with_name("real-bin")
                root.rename(relocated)
                root.symlink_to(relocated.name, target_is_directory=True)
            with self.subTest(mutation=mutation), mock.patch.object(export, "_tool_report") as report, \
                 self.assertRaisesRegex(ValueError, "build tool"):
                context._capture_build_tools()
            report.assert_not_called()

    def test_tool_size_mismatch_rejects_before_reading_replacement_bytes(self):
        context, root = self.context(capture=False)
        with (root / "cmake").open("ab") as stream: stream.write(b"larger untrusted executable")
        with mock.patch.object(portable, "byte_record") as read, \
             self.assertRaisesRegex(ValueError, "build tool bytes differ"):
            context._capture_build_tools()
        read.assert_not_called()

    def test_changed_tool_is_rejected_before_any_version_execution(self):
        for name in ("cmake", "ninja", "pkg_config"):
            context, root = self.context(capture=False)
            with (root / name).open("r+b") as stream:
                original = stream.read(1)
                stream.seek(0)
                stream.write(bytes([original[0] ^ 1]))
            with self.subTest(tool=name), mock.patch.object(export, "_tool_report") as report, \
                 self.assertRaisesRegex(ValueError, "build tool bytes differ"):
                context._capture_build_tools()
            report.assert_not_called()

    def test_missing_wrong_or_unmatched_pkg_config_record_cannot_probe_any_tool(self):
        for mutation in ("missing", "wrong-role", "path", "digest"):
            context, _root = self.context(capture=False)
            value = {"schema_version": 1, "tools": json.loads(json.dumps(context.producer_build_tools))}
            paths = {name: record["path"] for name, record in value["tools"].items()}
            if mutation == "missing": del value["tools"]["pkg_config"]
            elif mutation == "wrong-role": value["tools"]["pkg-config"] = value["tools"].pop("pkg_config")
            elif mutation == "path": value["tools"]["pkg_config"]["path"] = "/usr/bin/pkg-config"
            else: value["tools"]["pkg_config"]["sha256"] = "0" * 64
            with self.subTest(mutation=mutation), mock.patch.dict(export.PORTABLE_BUILD_TOOL_PATHS, paths, clear=True), \
                 mock.patch.object(export, "_tool_report") as report, self.assertRaises(ValueError):
                rejected = portable.PortableBuildContext({}, {}, {}, {}, build_tools=value)
                rejected._capture_build_tools()
            report.assert_not_called()

    def test_pkg_config_version_execution_is_fenced_before_receipt_acceptance(self):
        context, root = self.context(capture=False)
        report = export._tool_report
        executed = []
        def changed_pkg_config(command, **kwargs):
            result = report(command, **kwargs)
            if command[0] == str(root / "pkg_config"):
                executed.append(command)
                replacement = root / "pkg_config.replacement"
                shutil.copyfile(root / "pkg_config", replacement)
                replacement.chmod(0o700)
                replacement.replace(root / "pkg_config")
            return result
        with mock.patch.object(export, "_tool_report", side_effect=changed_pkg_config), \
             self.assertRaisesRegex(ValueError, "changed during version inspection"):
            context._capture_build_tools()
        self.assertEqual(executed, [[str(root / "pkg_config"), "--version"]])

    def test_admitted_bytes_must_report_exact_complete_producer_version(self):
        context, _root = self.context(capture=False)
        context.producer_build_tools["cmake"]["version"] += "unmatched producer output\n"
        with self.assertRaisesRegex(ValueError, "build tool version differs"):
            context._capture_build_tools()

    def test_actual_tool_drift_and_aliases_reject_existing_receipt(self):
        for name in ("cmake", "ninja", "pkg_config"):
            for mutation in ("bytes", "identity", "mode", "writable", "resolution"):
                context, root = self.context()
                path = root / name
                if mutation == "bytes":
                    with path.open("ab") as stream: stream.write(b"changed bytes")
                elif mutation in {"identity", "resolution"}:
                    replacement = path.with_suffix(".replacement")
                    shutil.copyfile(path, replacement)
                    replacement.chmod(0o700)
                    if mutation == "identity": replacement.replace(path)
                    else:
                        path.unlink()
                        path.symlink_to(replacement.name)
                else:
                    path.chmod(0o500 if mutation == "mode" else 0o720)
                with self.subTest(tool=name, mutation=mutation), self.assertRaisesRegex(ValueError, "build tool"):
                    context.verify_host_tools()

    def test_version_probe_and_ninja_query_are_fenced_after_execution(self):
        context, root = self.context(capture=False)
        report = export._tool_report
        def changed_version(command, **kwargs):
            result = report(command, **kwargs)
            with (root / "ninja").open("ab") as stream: stream.write(b"changed during version probe")
            return result
        with mock.patch.object(export, "_tool_report", side_effect=changed_version), \
             self.assertRaisesRegex(ValueError, "build tool bytes differ"):
            context._capture_build_tools()
        context, root = self.context()
        binary = root / "build"
        binary.mkdir()
        (binary / "build.ninja").write_text("rule fixture\n  command = /bin/echo fixture\nbuild atrinik: fixture\n")
        run = subprocess.run
        def changed_graph(command, **kwargs):
            result = run(command, **kwargs)
            with (root / "cmake").open("ab") as stream: stream.write(b"changed during graph query")
            return result
        with mock.patch.object(portable.subprocess, "run", side_effect=changed_graph), \
             self.assertRaisesRegex(ValueError, "build tool bytes differ"):
            context._ninja(binary)

    def test_export_revalidation_rejects_actual_tool_drift_before_cache_read(self):
        for name in ("cmake", "ninja", "pkg_config"):
            context, root = self.context()
            binary = root / "build"
            binary.mkdir()
            context.binary, context.source, context.baseline = binary, root, {}
            with (root / name).open("ab") as stream: stream.write(b"changed before export")
            with self.subTest(tool=name), self.assertRaisesRegex(WorkspaceError, "same-operation build receipt changed"):
                portable.validate_portable_build(binary, {}, context=context)


@unittest.skipUnless(sys.platform.startswith("linux"), "Linux portable compiler closure")
class CompilerClosureTests(unittest.TestCase):
    driver_evidence = lambda self: PortablePublicationTests.driver_evidence(self)
    driver_report = lambda self, command, **kwargs: PortablePublicationTests.driver_report(self, command, **kwargs)

    @staticmethod
    def build_tool_fixture(root):
        return fixture_build_tools(root)

    def setUp(self):
        temporary = tempfile.TemporaryDirectory()
        self.addCleanup(temporary.cleanup)
        self.root = Path(temporary.name)
        self.tools = self.build_tool_fixture(self.root / "host-tools")
        policy = mock.patch.object(portable.PortableBuildContext, "_build_tool_path_protected",
                                   side_effect=fixture_tool_path_protected)
        policy.start()
        self.addCleanup(policy.stop)

    def fixture(self, *, before_capture=None, after_bind=None, before_output=None):
        root = self.root / str(len(list(self.root.iterdir())))
        root.mkdir()
        selected, proofs, generations, views = {}, {}, {}, {}
        payloads = {
            "client": {"main.c": b"int core(void); int main(void) { return core(); }\n",
                       "src/include/client.h": b"#define CLIENT 1\n",
                       "shaders/SHA256SUMS": b"fixture pinned shader cohort\n",
                       "tools/embed_gpu_shaders.py": (b"def header_bytes(directory, manifest): return b'proof-bound shader\\n'\n"
                           b"if __name__ == '__main__':\n"
                           b" import pathlib, sys\n"
                           b" pathlib.Path(sys.argv[sys.argv.index('--output') + 1]).write_bytes(header_bytes(None, None))\n"),
                       },
            "libatrinik": {"core.c": b"int core(void) { return 0; }\n", "include/core.h": b"int core(void);\n"},
            "protocol": {"include/protocol.h": b"#define PROTOCOL 1\n"},
        }
        for role, files in payloads.items():
            origin = root / "generations" / role / "source"
            view = (root / "sources/client-layout/client" if role == "client" else root / "sources" / role / "input")
            selected[role], views[role] = origin, view
            generations[role] = {"source_includes": {"cmake": "cmake"}}
            proofs[role] = {}
            supplemental = b"# verified supplemental CMake module\n"
            for path in (origin.parent / "cmake/proof.cmake", view.parent / "cmake/proof.cmake"):
                path.parent.mkdir(parents=True, exist_ok=True)
                path.write_bytes(supplemental)
            proofs[role]["cmake/proof.cmake"] = {"sha256": hashlib.sha256(supplemental).hexdigest(), "size": len(supplemental)}
            if role == "client":
                toolchain = b"# proof-bound supplemental LLVM toolchain\n"
                for path in (origin.parent / "cmake/toolchains/llvm.cmake", view.parent / "cmake/toolchains/llvm.cmake"):
                    path.parent.mkdir(parents=True, exist_ok=True)
                    path.write_bytes(toolchain)
                proofs[role]["cmake/toolchains/llvm.cmake"] = {"sha256": hashlib.sha256(toolchain).hexdigest(), "size": len(toolchain)}
            for name, data in files.items():
                for path in (origin / name, view / name):
                    path.parent.mkdir(parents=True, exist_ok=True)
                    path.write_bytes(data)
                proofs[role]["source/" + name] = {"sha256": hashlib.sha256(data).hexdigest(), "size": len(data)}
        binary = root / "build/client"
        (binary / "src/include").mkdir(parents=True)
        (binary / "src/include/cmake.h").write_bytes(b"#define CMAKE_PROOF 1\n")
        library_headers = binary / "_deps/libatrinik-build/include"
        (library_headers / "toolkit").mkdir(parents=True)
        (library_headers / "toolkit/core.h").write_bytes(payloads["libatrinik"]["include/core.h"])
        (library_headers / "toolkit_cmake.h").write_bytes(b"#define LIBRARY_CMAKE_PROOF 1\n")
        common = [portable.LLVM_ROOT + "/bin/clang", *portable.PORTABLE_C_FLAGS.split(), "-O2", "-DNDEBUG"]
        main = [*common, "-I" + str(views["client"] / "src/include"), "-I" + str(binary / "src/include"),
                "-c", str(views["client"] / "main.c"), "-o", "main.o"]
        core = [*common, "-I" + str(library_headers), "-I" + str(views["libatrinik"] / "include"), "-c", str(views["libatrinik"] / "core.c"), "-o", "core.o"]
        link = [*common, *portable.PORTABLE_LINK_FLAGS.split(), "main.o", "core.a", "-o", "atrinik"]
        source = views["client"]
        cache = "".join(argument[2:].split("=", 1)[0] + ":STRING=" + argument.split("=", 1)[1] + "\n"
                        for argument in portable.portable_configure_arguments(source))
        (binary / "CMakeCache.txt").write_text(cache + "CMAKE_HOME_DIRECTORY:INTERNAL=" + str(source) + "\n")
        (binary / ".atrinik-configure.json").write_text(json.dumps({"source": {"path": str(source)}}))
        database = [{"arguments": words, "directory": str(binary), "file": words[words.index("-c") + 1]} for words in (main, core)]
        generator = "cd " + shlex.quote(str(binary)) + " && " + shlex.join([
            str(portable.PORTABLE_GENERATOR_PYTHON), str(source / "tools/embed_gpu_shaders.py"), "--input", "/opt/atrinik-portable/shaders",
            "--manifest", str(source / "shaders/SHA256SUMS"), "--output", str(binary / "src/include/gpu_shader_data.h")])
        archive = ": && /usr/bin/cmake -E rm -f core.a && " + portable.LLVM_ROOT + "/bin/llvm-ar qc core.a core.o && " + portable.LLVM_ROOT + "/bin/llvm-ranlib core.a && :"
        report = [generator, shlex.join(main), shlex.join(core), archive, ": && " + shlex.join(link) + " && :"]
        state = {"report": "\n".join(report) + "\n", "database": database, "binary": binary,
                 "main": main, "core": core, "views": views, "selected": selected, "proofs": proofs}
        if before_capture:
            before_capture(state)
        (binary / "compile_commands.json").write_text(json.dumps(state["database"]))
        evidence = self.driver_evidence()
        evidence["tools"].update({"archiver": {"path": portable.LLVM_ROOT + "/bin/llvm-ar"},
                                  "ranlib": {"path": portable.LLVM_ROOT + "/bin/llvm-ranlib"}})
        paths = {name: record["path"] for name, record in self.tools["tools"].items()}
        with mock.patch.dict(export.PORTABLE_BUILD_TOOL_PATHS, paths, clear=True):
            context = portable.PortableBuildContext(selected, proofs, generations, evidence,
                                                   build_tools=self.tools)
        state.update(context=context, evidence=evidence)
        context.validate_selected(selected)
        context.bind_source_views(root, views)
        (binary / ".atrinik-configure.json").write_text(json.dumps({"source": {"path": str(source)},
                                                                  "portable_host_tools": context.host_tools,
                                                                  "portable_build_tools": context.build_tools}))
        run_patch = mock.patch.object(portable.PortableBuildContext, "_ninja", side_effect=lambda path: state["report"])
        run_patch.start()
        self.addCleanup(run_patch.stop)
        def driver_with_real_missing_artifact_behavior(command, **kwargs):
            if len(command) == 2 and command[1] == "--version":
                return subprocess.run(command, check=True, capture_output=True, text=True, timeout=15,
                                      env={"PATH": os.defpath, "LC_ALL": "C"}).stdout
            if "-c" not in command and not (binary / "atrinik").exists():
                raise AssertionError("configure-only validation invoked linker before build artifacts existed")
            return self.driver_report(command, **kwargs)
        driver_patch = mock.patch.object(export, "_tool_report", side_effect=driver_with_real_missing_artifact_behavior)
        driver_patch.start()
        self.addCleanup(driver_patch.stop)
        if after_bind:
            after_bind(state)
        context.capture_configure(binary, source)
        if before_output:
            before_output(state)
        (binary / "src/include/gpu_shader_data.h").write_bytes(b"proof-bound shader\n")
        (binary / "atrinik").write_bytes(b"built fixture executable")
        context.capture_output(binary)
        return state

    def test_complete_off_graph_is_bound_to_caller_sources_and_build_receipt(self):
        state = self.fixture()
        toolchain = state["views"]["client"].parent / "cmake/toolchains/llvm.cmake"
        self.assertTrue(toolchain.is_file())
        self.assertIn(toolchain, state["context"].authored)
        self.assertFalse((state["views"]["client"] / "cmake").exists())
        self.assertIn("-DCMAKE_TOOLCHAIN_FILE=" + str(toolchain),
                      portable.portable_configure_arguments(state["views"]["client"]))
        self.assertIn(b"BUILD_TESTING:STRING=OFF", portable.validate_portable_build(
            state["binary"], state["evidence"], context=state["context"]))

    def test_real_generator_python_receipt_and_proof_ignore_caller_startup(self):
        modules = self.root / "caller-modules"
        modules.mkdir()
        marker = self.root / "python-startup-executed"
        (modules / "sitecustomize.py").write_text("import pathlib; pathlib.Path(" + repr(str(marker)) + ").touch()\n")
        subprocess.run(["/usr/bin/python3", "-c", "pass"], check=True, timeout=10,
                       env={"PATH": "/usr/bin:/bin", "PYTHONPATH": str(modules)})
        self.assertTrue(marker.exists())
        marker.unlink()
        with mock.patch.dict(os.environ, {"PYTHONPATH": str(modules), "PYTHONHOME": "/foreign/python-home",
                                          "PYTHONNOUSERSITE": "0", "HOME": str(self.root / "foreign-home")}):
            state = self.fixture()
        record = state["context"].host_tools["shader_python"]
        self.assertEqual(record["path"], "/usr/bin/python3")
        self.assertEqual(record["sha256"], hashlib.sha256(Path(record["resolved_path"]).read_bytes()).hexdigest())
        self.assertEqual(record["version"]["implementation"], "cpython")
        self.assertFalse(marker.exists())

    def test_actual_generator_executable_drift_fails_before_proof_output_and_export(self):
        original = Path("/usr/bin/python3").read_bytes()
        for phase in ("capture-probe", "before-proof", "before-output", "export", "alias"):
            executable = self.root / ("python-" + phase)
            executable.write_bytes(original)
            executable.chmod(0o700)
            def replace(state):
                replacement = executable.with_suffix(".replacement")
                replacement.write_bytes(original + b"changed interpreter storage")
                replacement.chmod(0o700)
                replacement.replace(executable)
            with self.subTest(phase=phase), mock.patch.object(portable, "PORTABLE_GENERATOR_PYTHON", executable):
                if phase == "capture-probe":
                    actual_run = subprocess.run
                    def changing_probe(command, **kwargs):
                        result = actual_run(command, **kwargs)
                        if "-c" in command:
                            replace(None)
                        return result
                    with mock.patch.object(portable.subprocess, "run", side_effect=changing_probe), \
                         self.assertRaisesRegex(ValueError, "Python changed during inspection"):
                        self.fixture()
                elif phase in {"before-proof", "before-output"}:
                    with self.assertRaisesRegex(ValueError, "Python executable changed"):
                        self.fixture(**({"after_bind": replace} if phase == "before-proof" else {"before_output": replace}))
                else:
                    state = self.fixture()
                    if phase == "alias":
                        alias_target = executable.with_suffix(".alias-target")
                        alias_target.write_bytes(original)
                        alias_target.chmod(0o700)
                        executable.unlink()
                        executable.symlink_to(alias_target)
                    else:
                        replace(state)
                    with self.assertRaisesRegex(WorkspaceError, "receipt changed"):
                        portable.validate_portable_build(state["binary"], state["evidence"], context=state["context"])

    def test_toolchain_requires_the_exact_supplemental_proof_member(self):
        def missing_proof(state):
            del state["proofs"]["client"]["cmake/toolchains/llvm.cmake"]
            toolchain = state["views"]["client"].parent / "cmake/toolchains/llvm.cmake"
            toolchain.unlink()
            toolchain.parent.rmdir()
        def fake_child(state):
            missing_proof(state)
            payload = b"# child toolchain cannot replace supplemental authority\n"
            for root in (state["views"]["client"], state["selected"]["client"]):
                path = root / "cmake/toolchains/llvm.cmake"
                path.parent.mkdir(parents=True)
                path.write_bytes(payload)
            state["proofs"]["client"]["source/cmake/toolchains/llvm.cmake"] = {
                "sha256": hashlib.sha256(payload).hexdigest(), "size": len(payload)}
        for mutation in (missing_proof, fake_child):
            with self.subTest(mutation=mutation.__name__), self.assertRaisesRegex(ValueError, "supplemental source proof"):
                self.fixture(before_capture=mutation)

    def test_fresh_target_scope_allows_proven_unrelated_library_database_target(self):
        def extra_target(state):
            words = list(state["core"])
            words[words.index("-o") + 1] = "unrelated-pathfinding-target.o"
            state["database"].append({"arguments": words, "directory": str(state["binary"]),
                                      "file": words[words.index("-c") + 1]})
        state = self.fixture(before_capture=extra_target)
        self.assertEqual(len(state["database"]), 3)
        self.assertEqual(state["context"].target_outputs,
                         {state["binary"] / "main.o", state["binary"] / "core.o"})
        portable.validate_portable_build(state["binary"], state["evidence"], context=state["context"])

    def test_external_source_and_header_controls_are_rejected_before_build(self):
        cases = [["-I/tmp/foreign"], ["-I", "/tmp/foreign"], ["-isystem", "/tmp/foreign"],
                 ["-iquote/tmp/foreign"], ["-idirafter", "/tmp/foreign"], ["-include", "/tmp/foreign.h"],
                 ["-imacros/tmp/foreign.h"], ["-include-pch", "/tmp/foreign.pch"], ["-Wp,-I,/tmp/foreign"],
                 ["-Xpreprocessor", "-include"], ["-ivfsoverlay", "/tmp/overlay"], ["-fmodules-cache-path=/tmp/modules"]]
        for extra in cases:
            def mutate(state, extra=extra):
                old = shlex.join(state["main"])
                state["main"].extend(extra)
                state["report"] = state["report"].replace(old, shlex.join(state["main"]))
            with self.subTest(extra=extra), self.assertRaises((WorkspaceError, ValueError)):
                self.fixture(before_capture=mutate)
        def external_tu(state):
            old = shlex.join(state["main"])
            state["main"][state["main"].index("-c") + 1] = "/tmp/evil.c"
            state["database"][0]["file"] = "/tmp/evil.c"
            state["report"] = state["report"].replace(old, shlex.join(state["main"]))
        with self.assertRaises(WorkspaceError):
            self.fixture(before_capture=external_tu)

    def test_effective_inputs_cannot_add_foreign_headers_or_change_main_source(self):
        state = self.fixture()
        source = state["views"]["client"] / "main.c"
        context, binary = state["context"], state["binary"]
        for extra in (["-internal-isystem", "/tmp/foreign"], ["-include", "/tmp/foreign.h"],
                      ["-imacros", "/tmp/foreign.h"], ["-include-pch", "/tmp/foreign.pch"],
                      ["-iwithprefixbefore", "/tmp/foreign"], ["-iframework", "/tmp/foreign"],
                      ["-fmodule-map-file=/tmp/foreign.modulemap"], ["-ivfsoverlay", "/tmp/overlay"]):
            with self.subTest(extra=extra), self.assertRaises(ValueError):
                context.check_inputs(["clang", "-cc1", *extra, str(source)], binary,
                                     effective=True, expected_source=source)
        with self.assertRaises(ValueError):
            context.check_inputs(["clang", "-cc1", "/tmp/evil.c"], binary,
                                 effective=True, expected_source=source)

    def test_unproven_aliases_and_build_translation_units_are_rejected(self):
        state = self.fixture()
        context, binary = state["context"], state["binary"]
        alias = self.root / "alias.c"
        alias.symlink_to(state["views"]["client"] / "main.c")
        generated = binary / "generated.c"
        generated.write_bytes(b"int main(void) { return 0; }\n")
        header_alias = self.root / "alias-headers"
        header_alias.symlink_to(state["views"]["client"] / "src/include", target_is_directory=True)
        for extra in (["-c", str(alias)], ["-c", str(generated)],
                      ["-c", str(state["views"]["client"] / "main.c"), "-I", str(header_alias)]):
            with self.subTest(extra=extra), self.assertRaises(ValueError):
                context.check_inputs(["clang", *extra], binary)

    def test_complete_database_and_transitive_link_consumption_are_required(self):
        def missing(state):
            state["report"] = state["report"].replace(shlex.join(state["core"]) + "\n", "")
        def unconsumed(state):
            state["report"] = state["report"].replace("main.o core.a -o", "main.o -o")
        for mutation in (missing, unconsumed):
            with self.subTest(mutation=mutation.__name__), self.assertRaises((WorkspaceError, ValueError)):
                self.fixture(before_capture=mutation)

    def test_shader_child_cannot_change_the_fresh_configure_baseline(self):
        def malicious_fixture_script(state):
            script = state["views"]["client"] / "tools/embed_gpu_shaders.py"
            payload = script.read_bytes().replace(b" import pathlib, sys\n",
                b" import pathlib, sys\n (pathlib.Path(__file__).parents[4] / 'build/client/compile_commands.json').write_text('[]')\n")
            script.write_bytes(payload)
            (state["selected"]["client"] / "tools/embed_gpu_shaders.py").write_bytes(payload)
            state["proofs"]["client"]["source/tools/embed_gpu_shaders.py"] = {
                "sha256": hashlib.sha256(payload).hexdigest(), "size": len(payload)}
        with self.assertRaisesRegex(WorkspaceError, "receipt changed"):
            self.fixture(before_capture=malicious_fixture_script)

    def test_configured_and_output_receipts_reject_mutation_and_joint_row_deletion(self):
        for mutation in ("rows", "source", "source-identity", "source-alias", "supplemental", "header", "header-alias", "copied-header", "library-shadow", "shadow", "executable", "executable-identity", "host-tool-cache", "cache-root"):
            state = self.fixture()
            binary, context = state["binary"], state["context"]
            if mutation == "rows":
                state["database"].pop()
                (binary / "compile_commands.json").write_text(json.dumps(state["database"]))
                state["report"] = state["report"].replace(shlex.join(state["core"]) + "\n", "")
            if mutation == "source": (state["views"]["client"] / "main.c").write_bytes(b"changed translation unit")
            if mutation == "source-identity":
                path = state["views"]["client"] / "main.c"
                temporary = path.with_suffix(".replacement")
                temporary.write_bytes(path.read_bytes())
                temporary.replace(path)
            if mutation == "source-alias":
                path = state["views"]["client"] / "main.c"
                external = self.root / "unproven.c"
                external.write_bytes(path.read_bytes())
                path.unlink()
                path.symlink_to(external)
            if mutation == "supplemental":
                (state["views"]["protocol"].parent / "cmake/proof.cmake").write_bytes(b"changed supplemental module")
            if mutation == "header": (binary / "src/include/cmake.h").write_bytes(b"changed configured header")
            if mutation == "header-alias":
                path = binary / "src/include/cmake.h"
                external = self.root / "unproven.h"
                external.write_bytes(path.read_bytes())
                path.unlink()
                path.symlink_to(external)
            if mutation == "copied-header":
                (binary / "_deps/libatrinik-build/include/toolkit/core.h").write_bytes(b"changed copied header")
            if mutation == "library-shadow":
                (binary / "_deps/libatrinik-build/include/stdio.h").write_bytes(b"shadow SDK header")
            if mutation == "shadow": (binary / "src/include/stdio.h").write_bytes(b"foreign shadow header")
            if mutation == "executable": (binary / "atrinik").write_bytes(b"replacement executable")
            if mutation == "executable-identity":
                path = binary / "atrinik"
                temporary = binary / "replacement"
                temporary.write_bytes(path.read_bytes())
                temporary.replace(path)
            if mutation == "host-tool-cache":
                path = binary / ".atrinik-configure.json"
                identity = json.loads(path.read_text())
                identity["portable_host_tools"]["shader_python"]["resolved_path"] = "/foreign/python3"
                path.write_text(json.dumps(identity))
            if mutation == "cache-root":
                source = str(state["views"]["client"])
                path = binary / "CMakeCache.txt"
                path.write_text(path.read_text().replace(source, "/tmp/foreign-client"))
                (binary / ".atrinik-configure.json").write_text(json.dumps({"source": {"path": "/tmp/foreign-client"}}))
            with self.subTest(mutation=mutation), self.assertRaises(WorkspaceError):
                portable.validate_portable_build(binary, state["evidence"], context=context)


@unittest.skipUnless(sys.platform.startswith("linux"), "Linux portable publication")
class PortablePublicationTests(unittest.TestCase):
    def setUp(self):
        inspector = shutil.which("llvm-readelf")
        if inspector:
            patcher = mock.patch.object(export, "LLVM_READELF", str(Path(inspector).absolute()))
            patcher.start()
            self.addCleanup(patcher.stop)
        self.temporary = tempfile.TemporaryDirectory()
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name)
        self.root.chmod(0o700)
        self.output = self.root / "client"
        self.sources = {"atrinik/classic@main": "a" * 40}

    def test_real_elf_cases_skip_when_only_gnu_readelf_is_available(self):
        import importlib.util
        spec = importlib.util.spec_from_file_location("missing_llvm_acceptance", __file__)
        module = importlib.util.module_from_spec(spec)
        with mock.patch("shutil.which", side_effect=lambda name: "/fixture/" + name
                        if name in {"clang", "readelf"} else None):
            spec.loader.exec_module(module)
        methods = [name for name in dir(module.PortablePublicationTests)
                   if getattr(getattr(module.PortablePublicationTests, name), "__unittest_skip_why__", "")
                   == "Clang and LLVM readelf required"]
        self.assertEqual(len(methods), 6)
        result = unittest.TestResult()
        unittest.TestSuite(module.PortablePublicationTests(name) for name in methods).run(result)
        self.assertEqual(len(result.skipped), 6)
        self.assertEqual(result.errors, [])

    def driver_evidence(self):
        return {"target": "x86_64-unknown-linux-gnu", "tools": {
                    "compiler": {"path": portable.LLVM_ROOT + "/bin/clang"},
                    "linker": {"path": portable.LLVM_ROOT + "/bin/ld.lld"}},
                "runtime": {"path": portable.LLVM_ROOT + "/lib/clang/23/lib/linux/libclang_rt.builtins-x86_64.a"},
                "unwinder": {"path": portable.LLVM_ROOT + "/lib/libunwind.a"}}

    def driver_report(self, command, **kwargs):
        evidence = self.driver_evidence()
        if "-c" in command:
            args = [str(Path(evidence["tools"]["compiler"]["path"]).resolve()), "-cc1", "-emit-obj",
                    "-triple", evidence["target"], "-target-cpu", "x86-64", "-tune-cpu", "generic",
                    "-resource-dir", portable.LLVM_ROOT + "/lib/clang/23", "-O2",
                    "-o", command[command.index("-o") + 1], command[command.index("-c") + 1]]
        else:
            args = [evidence["tools"]["linker"]["path"], "-m", "elf_x86_64",
                    "-o", command[command.index("-o") + 1],
                    "-L" + portable.LLVM_ROOT + "/lib", evidence["runtime"]["path"], "-lunwind",
                    *[word for word in command[1:] if word.endswith((".o", ".a"))]]
        return " ".join(json.dumps(arg) for arg in args) + "\n"

    def ninja_report(self, binary, source, compile_words, link_words, archive=None):
        generator = ["/usr/bin/python3", str(source / "tools/embed_gpu_shaders.py"),
                     "--input", "/opt/atrinik-portable/shaders", "--manifest", str(source / "shaders/SHA256SUMS"),
                     "--output", str(binary / "src/include/gpu_shader_data.h")]
        rows = ["cd " + shlex.quote(str(binary)) + " && " + shlex.join(generator), shlex.join(compile_words)]
        if archive:
            rows.append(archive)
        rows.append(": && " + shlex.join(link_words) + " && :")
        return "\n".join(rows) + "\n"

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

    def test_portable_build_refuses_compiler_optimization_linker_and_lto_drift(self):
        binary = self.root / "build"
        binary.mkdir()
        original = "".join(argument[2:].split("=", 1)[0] + ":STRING=" + argument.split("=", 1)[1] + "\n"
                           for argument in portable.portable_configure_arguments(self.root))
        original += "CMAKE_HOME_DIRECTORY:INTERNAL=" + str(self.root) + "\n"
        (binary / ".atrinik-configure.json").write_text(json.dumps({"source": {"path": str(self.root)}, "portable_host_tools": {}, "portable_build_tools": {}}))
        compile_words = [portable.LLVM_ROOT + "/bin/clang", "-O2", "-march=x86-64", "-mtune=generic",
                         "-O2", "-DNDEBUG", "--no-default-config", "-c", "fixture.c", "-o", "fixture.o"]
        link_words = [portable.LLVM_ROOT + "/bin/clang", "-O2", "-march=x86-64", "-mtune=generic",
                      "-O2", "-DNDEBUG", "--no-default-config", *portable.PORTABLE_LINK_FLAGS.split(), "fixture.o", "-o", "atrinik"]
        for case in ("valid", "compiler", "release", "cxx", "lto", "linker", "compile-o3", "link-o3", "response", "wrapper", "toolchain", "source", "user-registry", "system-registry", "missing-registry"):
            with self.subTest(case=case):
                cache, commands, link = original, list(compile_words), list(link_words)
                if case == "user-registry": cache = cache.replace("CMAKE_FIND_USE_PACKAGE_REGISTRY:STRING=FALSE", "CMAKE_FIND_USE_PACKAGE_REGISTRY:STRING=TRUE")
                if case == "system-registry": cache = cache.replace("CMAKE_FIND_USE_SYSTEM_PACKAGE_REGISTRY:STRING=FALSE", "CMAKE_FIND_USE_SYSTEM_PACKAGE_REGISTRY:STRING=TRUE")
                if case == "missing-registry": cache = cache.replace("CMAKE_FIND_USE_PACKAGE_REGISTRY:STRING=FALSE\n", "")
                if case == "compiler": cache = cache.replace("/bin/clang\n", "/bin/gcc\n")
                if case == "release": cache = cache.replace("-O2 -DNDEBUG", "-O3 -DNDEBUG")
                if case == "cxx": cache = cache.replace("CMAKE_CXX_FLAGS:STRING=-O2", "CMAKE_CXX_FLAGS:STRING=-O3")
                if case == "lto": commands.append("-flto=thin")
                if case == "linker": link.remove("-fuse-ld=lld")
                if case == "compile-o3": commands.append("-O3")
                if case == "link-o3": link.append("-O3")
                if case == "response": commands.append("@hidden-flags.rsp")
                if case == "wrapper": link.insert(0, "/unverified-wrapper")
                if case == "toolchain": cache = cache.replace(str(self.root.parent / "cmake/toolchains/llvm.cmake"), "/tmp/unverified.cmake")
                if case == "source": cache = cache.replace("CMAKE_HOME_DIRECTORY:INTERNAL=" + str(self.root), "CMAKE_HOME_DIRECTORY:INTERNAL=/tmp/other-source")
                (binary / "CMakeCache.txt").write_text(cache)
                (binary / "compile_commands.json").write_text(json.dumps([{"arguments": commands, "directory": str(binary), "file": commands[commands.index("-c") + 1]}]))
                with mock.patch.object(portable.subprocess, "run", return_value=
                    subprocess.CompletedProcess([], 0, stdout=self.ninja_report(binary, self.root, commands, link))), \
                     mock.patch.object(export, "_tool_report", side_effect=self.driver_report):
                    # This unit test isolates cached flag checks; actual caller
                    # source/receipt authority is covered by CompilerClosureTests.
                    context = mock.Mock(spec=portable.PortableBuildContext)
                    context.source = self.root
                    context.host_tools = {}
                    context.build_tools = {}
                    context.baseline = {"ninja": self.ninja_report(binary, self.root, commands, link)}
                    context.target_outputs = {binary / "fixture.o"}
                    if case == "valid": portable.validate_portable_build(binary, self.driver_evidence(), context=context)
                    else:
                        with self.assertRaises(WorkspaceError): portable.validate_portable_build(binary, self.driver_evidence(), context=context)

    def test_ninja_graph_binds_outputs_compilers_archives_and_generator(self):
        binary = self.root / "build"
        binary.mkdir()
        compiler = portable.LLVM_ROOT + "/bin/clang"
        compile_words = [compiler, "-o", "fixture.o", "-c", "fixture.c"]
        admitted = {binary / "fixture.o": compile_words}
        ar = portable.LLVM_ROOT + "/bin/llvm-ar"
        ranlib = portable.LLVM_ROOT + "/bin/llvm-ranlib"
        evidence = {"tools": {"archiver": {"path": ar}, "ranlib": {"path": ranlib}}}
        recipe = f": && /usr/bin/cmake -E rm -f owned.a && {ar} qc owned.a fixture.o && {ranlib} owned.a && :"
        link_words = [compiler, "owned.a", "-o", "atrinik"]
        normal = self.ninja_report(binary, self.root, compile_words, link_words, recipe)
        self.assertEqual(portable._portable_ninja_commands(normal, binary, evidence, admitted, self.root)[1],
                         {binary / "fixture.o", binary / "owned.a"})
        deps = normal.replace("-o fixture.o -c", "-MD -MT fixture.o -MF fixture.o.d -o fixture.o -c")
        portable._portable_ninja_commands(deps, binary, evidence, admitted, self.root)
        (binary / "escape").symlink_to(self.root)
        hostile = [normal.replace("-o atrinik", "-o /tmp/atrinik"),
                   recipe + "\n" + normal.replace(recipe + "\n", ""),
                   shlex.join(compile_words) + "\n" + normal.replace(shlex.join(compile_words) + "\n", ""),
                   ": && " + shlex.join(link_words) + " && :\n" + normal,
                   normal.replace("-o atrinik", "-o ../atrinik"),
                   normal.replace("-o atrinik", "-o escape/atrinik"),
                   normal.replace("-o atrinik", "-o nested/atrinik"),
                   normal.replace("-o atrinik", "-o atrinik -o other"),
                   normal.replace(ar, "/unrecorded/llvm-ar"),
                   normal.replace(ranlib, "/unrecorded/llvm-ranlib"),
                   normal.replace("rm -f owned.a", "rm -f other.a"),
                   normal.replace("qc owned.a", "qc ../owned.a"),
                   normal.replace("qc owned.a fixture.o", "qc owned.a unowned.o"),
                   normal.replace("qc owned.a fixture.o", "qc owned.a fixture.o fixture.o"),
                   normal.replace("qc owned.a fixture.o", "qc owned.a @members.rsp"),
                   normal.replace("qc owned.a", "qT owned.a"),
                   normal.replace(recipe, f"{ar} qc owned.a fixture.o\n{ranlib} owned.a"),
                   normal.replace(shlex.join(compile_words), "/tmp/foreign-clang -o fixture.o -c fixture.c"),
                   normal.replace("-o fixture.o", "-o fixture.o -oelsewhere.o"),
                   normal.replace(shlex.join(compile_words), ""),
                   deps.replace("-MT fixture.o", "-MT other.o"),
                   deps.replace("-MF fixture.o.d", "-MF foreign.d"),
                   normal + "/usr/bin/python3 -c \"open('fixture.o','wb').write(b'foreign')\"\n",
                   normal + "cp /tmp/foreign.o fixture.o\n",
                   normal.replace("fixture.c", "*.c"),
                   normal.replace("qc owned.a fixture.o", "qc owned.a *.o"),
                   normal.replace("cd " + str(binary), "cd /tmp"),
                   normal.replace("gpu_shader_data.h", "unowned.h"),
                   normal.replace("embed_gpu_shaders.py", "foreign.py"),
                   normal.replace("/opt/atrinik-portable/shaders --manifest", "/tmp/shaders --manifest"),
                   normal + shlex.join(compile_words) + "\n",
                   normal + ": && " + shlex.join(link_words) + " && :\n"]
        for report in hostile:
            with self.subTest(report=report), self.assertRaises((ValueError, KeyError)):
                portable._portable_ninja_commands(report, binary, evidence, admitted, self.root)
        # Core-library sources do not consume the client shader header.
        client_source = self.root / "client-source"
        core_words = [compiler, "-o", "fixture.o", "-c", str(self.root / "libatrinik/core.c")]
        core_report = self.ninja_report(binary, client_source, core_words, link_words, recipe)
        core_report = shlex.join(core_words) + "\n" + core_report.replace(shlex.join(core_words) + "\n", "")
        portable._portable_ninja_commands(core_report, binary, evidence, {binary / "fixture.o": core_words}, client_source)
        for output in ("./atrinik", str(binary / "atrinik")):
            portable._portable_ninja_commands(normal.replace("-o atrinik", "-o " + output),
                                             binary, evidence, admitted, self.root)
        orphan_link = [compiler, "missing.o", "-o", "atrinik"]
        with mock.patch.object(export, "_tool_report", side_effect=self.driver_report):
            with self.assertRaisesRegex(WorkspaceError, "effective compiler/linker contract changed"):
                portable._portable_driver(orphan_link, self.driver_evidence(), binary,
                                          link=True, admitted_build_inputs=set())
        spaced = self.root / "build with spaces"
        spaced.mkdir()
        portable._portable_ninja_commands(self.ninja_report(spaced, self.root, compile_words, link_words, recipe),
                                         spaced, evidence, {spaced / "fixture.o": compile_words}, self.root)
        with self.assertRaises(ValueError):
            portable._portable_compile_output([*compile_words, "-oelsewhere.o"], binary)
        alias = self.root / "alias"
        alias.symlink_to(binary, target_is_directory=True)
        with self.assertRaisesRegex(WorkspaceError, "not canonical"):
            portable.validate_portable_build(alias, {}, context=mock.Mock())
        with self.assertRaisesRegex(export.ExportError, "missing build input producer"):
            export.portable_link_inputs(["ld.lld", "missing.o"], binary=binary, admitted_build_inputs=set())
        for report in (["ld.lld", "-L", str(binary), "-lfoo"], ["ld.lld", "-L" + str(binary), "-lfoo"]):
            with self.assertRaises(export.ExportError):
                export.portable_library_paths(report, binary=None, installed=True)

    def test_effective_cxx_driver_uses_its_own_identity(self):
        evidence = self.driver_evidence()
        evidence["tools"]["cxx_compiler"] = {"path": str(self.root / "distinct-cxx")}
        words = [portable.LLVM_ROOT + "/bin/clang++", "-c", "fixture.cpp", "-o", "fixture.o"]
        c_report = self.driver_report(words)
        cxx_report = c_report.replace(str(Path(evidence["tools"]["compiler"]["path"]).resolve()),
                                     evidence["tools"]["cxx_compiler"]["path"])
        with mock.patch.object(export, "_tool_report", return_value=cxx_report):
            portable._portable_driver(words, evidence, self.root, link=False)
        with mock.patch.object(export, "_tool_report", return_value=c_report):
            with self.assertRaisesRegex(WorkspaceError, "effective compiler/linker contract changed"):
                portable._portable_driver(words, evidence, self.root, link=False)

    def test_actual_portable_flags_reject_additive_configuration_and_runtime_overrides(self):
        words = [portable.LLVM_ROOT + "/bin/clang", "-O2", "-march=x86-64", "-mtune=generic",
                 "-O2", "-DNDEBUG", "--no-default-config", *portable.PORTABLE_LINK_FLAGS.split(), "-o", "atrinik", "client.o"]
        portable._portable_command(words, {}, link=True)
        portable._portable_command([*words, *portable.PORTABLE_LINK_FLAGS.split()], {}, link=True)
        portable._portable_command([*words, "-Xlinker", "--dependency-file=atrinik.d"], {}, link=True)
        for flag in ("-fuse-ld=bfd", "--rtlib=libgcc", "--unwindlib=libgcc", "-msse4.2",
                     "--config=/tmp/other.cfg", "--config-user-dir=/tmp/config", "-mavx2",
                     "-B/tmp/unverified-tools", "-resource-dir=/tmp/unverified-runtime",
                     "--sysroot=/tmp/unverified-sysroot", "--gcc-toolchain=/tmp/unverified-gcc"):
            with self.subTest(flag=flag), self.assertRaises(WorkspaceError):
                portable._portable_command([*words, flag], {}, link=True)
        with self.assertRaises(WorkspaceError):
            portable._portable_command([words[0], "-L/tmp/unverified-runtime", *words[1:]], {}, link=True)
        for option in (["-L/tmp/unverified-runtime"], ["-L", "/tmp/unverified-runtime"],
                       ["--library-path=/tmp/unverified-runtime"], ["-L/tmp/foreign/../../usr/lib"]):
            with self.subTest(option=option), self.assertRaises(WorkspaceError):
                portable._portable_command([*words, *option], {}, link=True)
        portable._portable_command([*words, "-L/usr/local/lib"], {}, link=True)
        portable._portable_command([*words, "-L", str(self.root / "owned")], {}, link=True, binary=self.root)
        outside = self.root.parent / "outside"
        (self.root / "escape").symlink_to(outside)
        with self.assertRaises(WorkspaceError):
            portable._portable_command([*words, "-L", str(self.root / "escape")], {}, link=True, binary=self.root)
        with self.assertRaises(WorkspaceError):
            portable._portable_command([*words, "-Wl,-L,/tmp/unverified-runtime"], {}, link=True)
        with self.assertRaises(WorkspaceError):
            portable._portable_command([*words, "-Xlinker", "-L/tmp/unverified-runtime"], {}, link=True)
        with self.assertRaises(WorkspaceError):
            portable._portable_command(["/tmp/unverified-launcher", *words],
                                       {"CMAKE_C_COMPILER_LAUNCHER": "/tmp/unverified-launcher"}, link=True)
        with self.assertRaises(WorkspaceError):
            portable._portable_command([word for word in words if word != "--no-default-config"], {}, link=True)

    def test_effective_driver_refuses_external_tools_and_codegen_runtime_changes(self):
        evidence = self.driver_evidence()
        compile_words = [portable.LLVM_ROOT + "/bin/clang", "-c", "fixture.c", "-o", "fixture.o"]
        link_words = [portable.LLVM_ROOT + "/bin/clang", "fixture.o", "-o", "atrinik"]
        cc1 = self.driver_report(compile_words)
        linker = self.driver_report(link_words)
        cases = [(cc1, False, True), (linker, True, True),
                 (linker.replace('"atrinik"', '"/tmp/atrinik"'), True, False),
                 (linker.rstrip() + ' "-o" "atrinik"\n', True, False),
                 (linker.rstrip() + ' "--output=/tmp/atrinik"\n', True, False),
                 (cc1.replace('"x86-64"', '"haswell"'), False, False),
                 (cc1.replace('"fixture.o"', '"elsewhere.o"'), False, False),
                 (cc1.rstrip() + ' "-oelsewhere.o"\n', False, False),
                 (cc1.rstrip() + ' "--output=elsewhere.o"\n', False, False),
                 (cc1.replace('"-O2"', '"-O3"'), False, False),
                 (cc1.replace('/lib/clang/23"', '/foreign-runtime"'), False, False),
                 (cc1.rstrip() + ' "-target-feature" "+avx2"\n', False, False),
                 (cc1.rstrip() + ' "-load" "/tmp/plugin.so"\n', False, False),
                 (cc1.rstrip() + ' "-fpass-plugin=/tmp/plugin.so"\n', False, False),
                 (cc1 + '"/usr/bin/as" "fixture.s"\n', False, False),
                 (linker.replace('/bin/ld.lld', '/bin/foreign-ld'), True, False),
                 (linker.replace('"-lunwind"', ''), True, False),
                 (linker.rstrip() + ' "-L/tmp/foreign"\n', True, False),
                 (linker.rstrip() + ' "/tmp/foreign/libhidden.a"\n', True, False),
                 (linker.rstrip() + ' "--version-script=/tmp/foreign.lds"\n', True, False),
                 (linker.rstrip() + ' "-L" "/tmp/foreign"\n', True, False),
                 (linker.rstrip() + ' "--library-path=/tmp/foreign"\n', True, False),
                 (cc1.replace('"-cc1"', '"--forged" "-cc1"'), False, False),
                 (linker.replace(evidence["runtime"]["path"], '/tmp/foreign-builtins.a'), True, False),
                 (linker.replace('"-L' + portable.LLVM_ROOT, '"-L/tmp/foreign'), True, False)]
        for option in ('"-l:../../../tmp/foreign/libhidden.a"', '"-l" ":../../../tmp/foreign/libhidden.a"',
                       '"--library=../../../tmp/foreign/libhidden.a"', '"--library" "../../../tmp/foreign/libhidden.a"',
                       '"--remap-inputs=owned.o=/tmp/foreign.o"', '"--remap-inputs-file=/tmp/map"',
                       '"-R/tmp/foreign.o"'):
            cases.append((linker.rstrip() + " " + option + "\n", True, False))
        for output, link, valid in cases:
            with self.subTest(output=output), mock.patch.object(export, "_tool_report", return_value=output):
                words = link_words if link else compile_words
                if valid: portable._portable_driver(words, evidence, self.root, link=link)
                else:
                    with self.assertRaises(WorkspaceError): portable._portable_driver(words, evidence, self.root, link=link)

    @unittest.skipUnless(shutil.which("clang"), "Clang driver fixture")
    def test_actual_llvm_driver_rejects_foreign_codegen_tools_and_missing_runtimes(self):
        compiler = Path(shutil.which("clang")).absolute()
        version = subprocess.check_output([str(compiler), "--version"], text=True)
        prefix = compiler.resolve().parent.parent
        linker = prefix / "bin/ld.lld"
        if "clang version 23.1.3" not in version or not linker.is_file():
            self.skipTest("exact LLVM 23.1.3 compiler/linker fixture required")
        source = self.root / "driver.c"
        source.write_text("int main(void) { return 0; }\n")
        object_file = self.root / "driver.o"
        subprocess.run([str(compiler), "--no-default-config", "-c", str(source), "-o", str(object_file)], check=True)
        evidence = self.driver_evidence()
        evidence["tools"]["compiler"]["path"] = str(compiler)
        evidence["tools"]["linker"]["path"] = str(linker)
        evidence["target"] = subprocess.check_output([str(compiler), "--no-default-config", "-dumpmachine"], text=True).strip()
        evidence["runtime"]["path"] = subprocess.check_output([str(compiler), "--no-default-config", "--rtlib=compiler-rt", "--print-libgcc-file-name"], text=True).strip()
        evidence["unwinder"]["path"] = str(prefix / "lib/libunwind.a")
        base = [str(compiler), "--no-default-config", "-O2", "-march=x86-64", "-mtune=generic"]
        compile_words = [*base, "-c", str(source), "-o", str(object_file)]
        link_words = [*base, "-L" + str(prefix / "lib"), "--rtlib=compiler-rt", "--unwindlib=libunwind",
                      "-fuse-ld=lld", str(object_file), "-o", str(self.root / "driver")]
        # This real-driver fixture may run on the Ubuntu test worker. Its
        # system library paths are fixture inputs; the separate frozen Debian
        # report tests the production producer's exact directory contract.
        report = export._tool_report([link_words[0], "-###", *link_words[1:]], cwd=self.root)
        import shlex
        default_spellings = {arg[2:] for line in report.splitlines()
                               if line.lstrip().startswith('"') for arg in shlex.split(line)
                               if arg.startswith("-L")}
        default_directories = {os.path.normpath(path) for path in default_spellings}
        with mock.patch.object(portable, "LLVM_ROOT", str(prefix)), \
             mock.patch.object(export, "PORTABLE_LIBRARY_DIRECTORIES", default_directories | export.PORTABLE_LIBRARY_DIRECTORIES), \
             mock.patch.object(export, "PORTABLE_LIBRARY_SPELLINGS", default_spellings | export.PORTABLE_LIBRARY_SPELLINGS):
            portable._portable_driver(compile_words, evidence, self.root, link=False)
            portable._portable_driver(link_words, evidence, self.root, link=True)
            cases = [(compile_words + ["-fno-integrated-as"], False),
                     (compile_words + ["-resource-dir=/tmp/foreign-resource"], False),
                     (compile_words + ["--target=aarch64-linux-gnu"], False),
                     (compile_words + ["-mllvm", "-force-vector-width=8"], False),
                     (link_words + ["-nostdlib"], True), (link_words + ["-nodefaultlibs"], True),
                     (link_words + ["-L/tmp/foreign"], True),
                     (link_words + ["/tmp/foreign/libhidden.a"], True)]
            for words, link in cases:
                with self.subTest(words=words), self.assertRaises(WorkspaceError):
                    portable._portable_driver(words, evidence, self.root, link=link)

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

    @unittest.skipUnless(shutil.which("clang"), "native compiler")
    def test_real_launcher_helpers_are_separate_from_client_loader_trace(self):
        import runpy
        namespace = runpy.run_path(str(Path(__file__).resolve().parents[1] / "scripts/linux_portable_acceptance.py"))
        for path in ("bin", "lib", "share/games/atrinik"):
            (self.output / path).mkdir(parents=True)
        source = self.root / "client.c"
        source.write_text('#include <stdio.h>\nint main(void) { puts("client help"); return 0; }\n')
        subprocess.run(["clang", str(source), "-o", str(self.output / "bin/atrinik")],
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

    @unittest.skipUnless(shutil.which("clang"), "native compiler")
    def test_real_loader_rejects_omitted_application_library_even_without_manifest_entry(self):
        import runpy
        namespace = runpy.run_path(str(Path(__file__).resolve().parents[1] / "scripts/linux_portable_acceptance.py"))
        source = self.root / "consumer.c"
        source.write_text("extern const char *zlibVersion(void); int main(void) { return !zlibVersion(); }\n")
        binary = self.root / "consumer"
        subprocess.run(["clang", str(source), "-o", str(binary), "-l:libz.so.1"], check=True, capture_output=True)
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

    def test_export_source_copy_and_final_publication_guards(self):
        build_tools = fixture_build_tools(self.root / "metadata-tools")
        from scripts.linux_portable_acceptance import SOUND_RELEASE
        from atrinik_workspace import sound
        cases = {"complete": None, "dirty": "clean committed", "classic": "requires Classic",
                 "sound": "selected sound source commit", "generation": "immutable primary",
                 "configure": "CMake configuration changed", "sound-tree": "source tree",
                 "source-bytes": "source proof changed", "late-record": "source generation changed",
                 "late-proof": "source closure changed", "late-sound": "released sound changed",
                 "late-metadata": "producer metadata changed", "late-configure": "build configuration changed"}
        for case, failure in cases.items():
            with self.subTest(case=case), ExitStack() as patches:
                directory = self.root / case
                directory.mkdir()
                destination = directory / "published"
                selected, records, states = {}, {}, {}
                for role, checkout in (("client", "classic"), ("sound", "sound")):
                    source = directory / role / "source"
                    source.mkdir(parents=True)
                    (source / "LICENSE.md").write_bytes(b"original source license")
                    (source / "media.dat").write_bytes(b"materialized source data")
                    selected[role] = source
                    records[role] = {"checkout": checkout, "source_tree": "b" * 40,
                                     "tree": "c" * 40, "source_includes": ["."]}
                    states[checkout] = {"dirty": False, "path": str(directory / checkout),
                                        "head": portable.CONSUMER_COMMIT if checkout == "classic"
                                        else SOUND_RELEASE["source_commit"]}
                (selected["client"] / "shaders").mkdir()
                (selected["client"] / "shaders/source.hlsl").write_bytes(b"source only")
                binary = directory / "build/client"
                binary.mkdir(parents=True)
                cache = binary / "CMakeCache.txt"
                cache.write_text("".join(argument[2:].split("=", 1)[0] + ":STRING=" + argument.split("=", 1)[1] + "\n"
                                         for argument in portable.portable_configure_arguments(self.root)))
                with cache.open("a") as stream:
                    stream.write("CMAKE_HOME_DIRECTORY:INTERNAL=" + str(self.root) + "\n")
                (binary / ".atrinik-configure.json").write_text(json.dumps({"source": {"path": str(self.root)}}))
                compile_words = [portable.LLVM_ROOT + "/bin/clang", "-O2", "-march=x86-64", "-mtune=generic",
                                 "-O2", "-DNDEBUG", "--no-default-config", "-c", "fixture.c", "-o", "fixture.o"]
                (binary / "compile_commands.json").write_text(json.dumps([{"arguments": compile_words, "directory": str(binary), "file": compile_words[compile_words.index("-c") + 1]}]))
                link_words = [portable.LLVM_ROOT + "/bin/clang", "-O2", "-march=x86-64", "-mtune=generic",
                              "-O2", "-DNDEBUG", "--no-default-config", *portable.PORTABLE_LINK_FLAGS.split(), "fixture.o", "-o", "atrinik"]
                patches.enter_context(mock.patch.object(portable.subprocess, "run", return_value=
                    subprocess.CompletedProcess([], 0, stdout=self.ninja_report(binary, self.root, compile_words, link_words))))
                released = directory / "released"
                released.mkdir()
                (released / "COPYING").write_bytes(b"released sound attribution")
                (released / "effect.ogg").write_bytes(b"encoded fixture")
                sound_record = {"source_tree": "b" * 40, "root": str(released)}
                snapshot = mock.Mock()
                snapshot.paths.return_value = selected
                snapshot.checkout_states.return_value = states
                workspace = mock.Mock()
                workspace._load_profile.return_value = {"stack": "classic", "sound_mode": "released",
                                                         "sound_release": SOUND_RELEASE}
                workspace._resolved_profile_operation.return_value = nullcontext(snapshot)
                workspace._profile_build_lock.return_value = nullcontext()
                workspace._expand_build_target.return_value = ["client"]
                workspace._classic_binary_directory.return_value = binary
                workspace._prepare_sound.return_value = (released, sound_record)
                def generation(source):
                    role = next(role for role, path in selected.items() if path == source)
                    return None if case == "generation" else json.loads(json.dumps(records[role]))
                workspace._source_generation_record.side_effect = generation
                def closure(checkout, root, *arguments):
                    result = {}
                    for path in sorted(root.rglob("*")):
                        if path.is_file():
                            raw = path.read_bytes()
                            result[str(path.relative_to(root))] = {
                                "sha256": hashlib.sha256(raw).hexdigest(), "size": len(raw),
                                "executable": bool(path.stat().st_mode & 0o111)}
                    return result
                workspace._validate_source_generation_git_closure.side_effect = closure
                def build(*arguments, **options):
                    if case == "source-bytes":
                        (selected["client"] / "media.dat").write_bytes(b"changed source")
                    return binary.parent
                workspace._build_resolved.side_effect = build
                # Isolate export's source/publication guards from compilation;
                # CompilerClosureTests exercises the real build receipt API.
                patches.enter_context(mock.patch.object(portable, "PortableBuildContext",
                                                        return_value=mock.Mock(binary=binary, host_tools={}, build_tools={})))
                def validated_cache(*args, **kwargs):
                    data = cache.read_bytes()
                    if case == "configure":
                        raise WorkspaceError("portable CMake configuration changed")
                    return data
                patches.enter_context(mock.patch.object(portable, "validate_portable_build", side_effect=validated_cache))
                def runtime(publication, documents, executable):
                    self.assertEqual(executable, binary / "atrinik")
                    publication.add_bytes("bin/atrinik", b"fixture client", executable=True)
                    publication.add_bytes("lib/provider.so", b"fixture provider")
                    publication.manifest["application_libraries"].append("lib/provider.so")
                    return {"derivatives": [{"fixture": "exact input normalization"}]}
                def legal(publication, documents):
                    publication.add_bytes("licenses/provider", b"provider notice", notice=True)
                    return {"sources_and_notices_materialized": True}
                def verify_sound(*arguments):
                    if case == "late-record": records["client"]["tree"] = "d" * 40
                    if case == "late-proof": (selected["client"] / "media.dat").write_bytes(b"late change")
                    if case == "late-configure": cache.write_text("changed configure contract")
                    return {**sound_record, "changed": True} if case == "late-sound" else sound_record
                if case == "dirty": states["classic"]["dirty"] = True
                if case == "classic": states["classic"]["head"] = "a" * 40
                if case == "sound": states["sound"]["head"] = "a" * 40
                if case == "configure": cache.write_text("CMAKE_SKIP_RPATH:BOOL=OFF\n")
                if case == "sound-tree": sound_record["source_tree"] = "e" * 40
                patches.enter_context(mock.patch.object(export, "_tool_report", side_effect=self.driver_report))
                metadata = ({"contract": b"authenticated fixture"}, {"compiler-evidence.json": self.driver_evidence(),
                            "installed.json": {"build_tools": build_tools}})
                patches.enter_context(mock.patch.object(portable, "installed_metadata", side_effect=[
                    metadata, ({"contract": b"changed"}, {}) if case == "late-metadata" else metadata]))
                patches.enter_context(mock.patch.object(portable, "add_runtime", side_effect=runtime))
                patches.enter_context(mock.patch.object(portable, "add_corresponding_sources", side_effect=legal))
                patches.enter_context(mock.patch.object(sound, "verify_release_tree", side_effect=verify_sound))
                if failure:
                    with self.assertRaisesRegex((WorkspaceError, export.ExportError), failure):
                        portable.export_client(workspace, "qualified", destination)
                    self.assertFalse(destination.exists())
                    if case in ("dirty", "classic", "sound", "generation"):
                        workspace._build_resolved.assert_not_called()
                else:
                    result = portable.export_client(workspace, "qualified", destination)
                    export.verify_export(destination)
                    self.assertEqual(result["image"], portable.IMAGE)
                    self.assertEqual((destination / "share/games/atrinik/media.dat").read_bytes(),
                                     b"materialized source data")
                    self.assertTrue((destination / "sources/client/source/shaders/source.hlsl").is_file())
                    self.assertFalse((destination / "share/games/atrinik/shaders/source.hlsl").exists())
                    self.assertEqual((destination / "share/games/atrinik/sound/effect.ogg").read_bytes(),
                                     b"encoded fixture")
                    self.assertIn("licenses/libpulse-export-modification.txt",
                                  export.verify_export(destination)["notices"])
                    self.assertIn(b"OPENSSL_MODULES", (destination / "atrinik").read_bytes())

    def test_corresponding_source_and_notice_closure_is_materialized_or_refused(self):
        copyright_path = Path("/usr/share/doc/bash/copyright").resolve(strict=True)
        common_names = ("GPL-2", "LGPL-2.1")
        def record(path):
            data = path.read_bytes()
            return {"sha256": hashlib.sha256(data).hexdigest(), "size": len(data),
                    "executable": bool(path.stat().st_mode & 0o111)}
        for case in ("complete", "missing-notices", "wrong-copyright", "wrong-common", "escape"):
            with self.subTest(case=case), ExitStack() as patches:
                producer = self.root / case / "producer"
                (producer / "sources/debian").mkdir(parents=True)
                archive = producer / "sources/debian/package.orig.tar.xz"
                archive.write_bytes(b"original Debian source archive")
                upstream_bytes = b"original upstream source archive"
                upstream_sha = hashlib.sha256(upstream_bytes).hexdigest()
                (producer / "sources" / (upstream_sha + ".tar.gz")).write_bytes(upstream_bytes)
                if case != "missing-notices":
                    (producer / "notices").mkdir()
                    (producer / "notices/LICENSE").write_bytes(b"producer attribution")
                producer_records = {str(path.relative_to(producer)): record(path)
                                    for path in producer.rglob("*") if path.is_file()}
                notices = {"/usr/share/doc/bash/copyright": {**record(copyright_path),
                                                            "resolved_path": str(copyright_path)}}
                common = {name: record(Path("/usr/share/common-licenses") / name) for name in common_names}
                documents = {"runtime-sources.json": {"source_packages": ["bash"],
                             "archives": {archive.name: record(archive)["sha256"]}},
                             "contract.json": {"sources": [{"url": "https://example.invalid/upstream.tar.gz",
                                                               "sha256": upstream_sha}]},
                             "debian-sources.json": [
                                 {"source": "ignored", "package": "ignored", "notice_directory": "/unavailable"},
                                 {"source": "bash", "package": "bash:amd64", "notice_directory": "/usr/share/doc/bash"}]}
                if case == "escape":
                    (producer / "copyright").write_bytes(b"outside Debian notice boundary")
                    documents["debian-sources.json"][1]["notice_directory"] = str(producer)
                    producer_records["copyright"] = record(producer / "copyright")
                patches.enter_context(mock.patch.object(portable, "Path", side_effect=lambda value:
                    producer if str(value) == "/opt/atrinik-portable" else Path(value)))
                patches.enter_context(mock.patch.object(portable, "PRODUCER_FILES_SHA256",
                                                        portable.inventory_digest(producer_records)))
                patches.enter_context(mock.patch.object(portable, "DEBIAN_NOTICES_SHA256",
                    "0" * 64 if case == "wrong-copyright" else portable.inventory_digest(notices)))
                patches.enter_context(mock.patch.object(portable, "COMMON_LICENSE_NAMES", common_names))
                patches.enter_context(mock.patch.object(portable, "COMMON_LICENSES_SHA256",
                    "0" * 64 if case == "wrong-common" else portable.inventory_digest(common)))
                # Archive-member notice materialization has separate exact-byte tests.
                patches.enter_context(mock.patch.object(portable, "add_audio_notices", return_value=[]))
                destination = self.root / case / "published"
                with portable.Publication(destination, self.sources) as publication:
                    self.populate(publication)
                    if case != "complete":
                        with self.assertRaisesRegex(export.ExportError, "portable-legal"):
                            portable.add_corresponding_sources(publication, documents)
                        self.assertFalse(destination.exists())
                    else:
                        result = portable.add_corresponding_sources(publication, documents)
                        publication.publish(lambda: None)
                        manifest = export.verify_export(destination)
                        self.assertEqual(result["debian_archives"], 1)
                        self.assertEqual(result["debian_source_packages"], 1)
                        self.assertEqual((destination / "sources/debian" / archive.name).read_bytes(), archive.read_bytes())
                        self.assertEqual((destination / "sources/upstream/upstream.tar.gz").read_bytes(), upstream_bytes)
                        self.assertEqual((destination / "licenses/debian/bash_amd64/copyright").read_bytes(), copyright_path.read_bytes())
                        self.assertIn("sources/portable-producer/notices/LICENSE", manifest["notices"])

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

    @unittest.skipUnless(shutil.which("clang") and shutil.which("llvm-readelf"), "Clang and LLVM readelf required")
    def test_portable_executable_rejects_custom_or_missing_loader(self):
        source = self.root / "main.c"
        source.write_text("int main(void) { return 0; }\n")
        for name, options in (("normal", []), ("custom", ["-Wl,--dynamic-linker=/tmp/private-loader"]),
                              ("missing", ["-shared", "-fPIC"])):
            target = self.root / name
            subprocess.run(["clang", str(source), "-o", str(target), *options], check=True, capture_output=True)
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

    @unittest.skipUnless(shutil.which("clang") and shutil.which("llvm-readelf"), "Clang and LLVM readelf required")
    def test_real_versioned_symbol_definitions_and_missing_symbol(self):
        source = self.root / "sample.c"
        version = self.root / "version.map"
        library = self.root / "libsample.so"
        source.write_text("int sample(void) { return 7; }\n")
        version.write_text("SAMPLE_1 { global: sample; local: *; };\n")
        subprocess.run(["clang", "-shared", "-fPIC", "-nostdlib", str(source), "-o", str(library),
                        "-Wl,--version-script=" + str(version)], check=True, capture_output=True)
        with library.open("rb") as stream:
            facts = export.inspect_elf(stream.fileno())
        self.assertIsNone(facts["soname"])
        self.assertEqual(facts["defined_versions"], ["SAMPLE_1"])
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

    def test_qualified_bootstrap_selects_pinned_commit_after_main_advances(self):
        ns = self.acceptance_namespace()
        upstream = self.root / "upstream"
        upstream.mkdir()
        def git(*args):
            return subprocess.run(["git", *map(str, args)], check=True, text=True,
                                  capture_output=True).stdout.strip()
        git("-C", upstream, "init", "--initial-branch=main")
        git("-C", upstream, "config", "user.name", "Fixture")
        git("-C", upstream, "config", "user.email", "fixture@example.invalid")
        (upstream / "source").write_text("qualified\n")
        git("-C", upstream, "add", "source")
        git("-C", upstream, "-c", "commit.gpgsign=false", "commit", "-m", "qualified")
        qualified = git("-C", upstream, "rev-parse", "HEAD")
        (upstream / "source").write_text("new main\n")
        git("-C", upstream, "-c", "commit.gpgsign=false", "commit", "-am", "advance")
        self.assertNotEqual(qualified, git("-C", upstream, "rev-parse", "HEAD"))
        workspace = self.root / "workspace"
        workspace.mkdir()
        wrapper_calls = []
        def local_run(arguments):
            if arguments[0] == "./atrinik":
                wrapper_calls.append(arguments)
                return ""
            if "fetch" in arguments:
                # Redirect only fixture transport; preserve the canonical origin.
                arguments = ["git", "-c", "url." + str(upstream) + ".insteadOf=https://github.com/atrinik/classic.git",
                             "-c", "url." + str(upstream) + ".insteadOf=https://github.com/atrinik/sound.git",
                             *arguments[1:]]
            return ns["run"](arguments)
        with chdir(workspace), mock.patch.dict(ns["prepare_sources"].__globals__, {
                "run": local_run, "SOURCE_COMMITS": {"classic": qualified, "sound": qualified}}):
            ns["prepare_sources"]()
        self.assertEqual(len(wrapper_calls), 2)
        for name in ("classic", "sound"):
            repo = workspace / name
            self.assertEqual(git("-C", repo, "rev-parse", "HEAD"), qualified)
            self.assertEqual(git("-C", repo, "branch", "--show-current"), "main")
            self.assertEqual(git("-C", repo, "status", "--porcelain"), "")
            self.assertEqual(git("-C", repo, "remote", "get-url", "origin"),
                             "https://github.com/atrinik/" + name + ".git")
            self.assertEqual((repo / "source").read_text(), "qualified\n")

    def test_qualified_bootstrap_refuses_all_occupied_destinations_before_writing(self):
        ns = self.acceptance_namespace()
        for kind in ("directory", "file", "symlink"):
            with self.subTest(kind=kind), tempfile.TemporaryDirectory(dir=self.root) as directory:
                root = Path(directory)
                target = root / "sound"
                if kind == "directory":
                    target.mkdir()
                elif kind == "file":
                    target.write_text("preserve me")
                else:
                    target.symlink_to(root / "absent")
                runner = mock.Mock()
                with chdir(root), mock.patch.dict(ns["prepare_sources"].__globals__, {"run": runner}):
                    with self.assertRaisesRegex(RuntimeError, "already exists: sound"):
                        ns["prepare_sources"]()
                runner.assert_not_called()
                self.assertFalse((root / "classic").exists())
                self.assertTrue(target.exists() or target.is_symlink())

    def test_qualified_bootstrap_fetch_failure_stops_before_export_or_repinning(self):
        ns = self.acceptance_namespace()
        calls = []
        def fail_fetch(arguments):
            calls.append(arguments)
            if "fetch" in arguments:
                raise subprocess.CalledProcessError(128, arguments)
            return ""
        with chdir(self.root), mock.patch.dict(ns["prepare_sources"].__globals__, {"run": fail_fetch}):
            with self.assertRaises(subprocess.CalledProcessError):
                ns["prepare_sources"]()
        self.assertEqual(calls[-1][-1], portable.CONSUMER_COMMIT)
        self.assertFalse(any("checkout" in call or call[0] == "./atrinik" for call in calls))
        self.assertFalse((self.root / "sound").exists())

    def test_qualified_bootstrap_rechecks_identity_and_cleanliness_after_init(self):
        for failure in ("head", "dirty"):
            with self.subTest(failure=failure), tempfile.TemporaryDirectory(dir=self.root) as directory:
                ns = self.acceptance_namespace()
                calls = []
                initialized = False

                def run(arguments):
                    nonlocal initialized
                    calls.append(arguments)
                    if arguments[:2] == ["./atrinik", "init"]:
                        initialized = True
                    if "rev-parse" in arguments:
                        self.assertTrue(initialized)
                        return "f" * 40 if failure == "head" else ns["SOURCE_COMMITS"][arguments[2]]
                    if "status" in arguments:
                        self.assertTrue(initialized)
                        return " M source\n"
                    return ""

                with chdir(directory), mock.patch.dict(ns["prepare_sources"].__globals__, {"run": run}):
                    message = "qualified source changed" if failure == "head" else "dirty dependency"
                    with self.assertRaisesRegex(RuntimeError, message):
                        ns["prepare_sources"]()
                self.assertFalse(any(call[:2] == ["./atrinik", "profile"] for call in calls))

    def test_acceptance_pinned_primaries_produce_immutable_export_sources(self):
        from atrinik_workspace.workspace import Workspace
        ns = self.acceptance_namespace()
        origins, pins, tips = {}, {}, {}
        shutil.copyfile(Path(__file__).parents[1] / "components.json", self.root / "components.json")

        def git(root, *arguments):
            return subprocess.run(["git", "-C", str(root), *arguments], check=True,
                                  capture_output=True, text=True).stdout

        for name in ("classic", "sound"):
            root = self.root / (name + "-origin")
            root.mkdir()
            origins[name] = root
            git(root, "init", "-b", "main")
            git(root, "config", "user.name", "Tests")
            git(root, "config", "user.email", "tests@example.invalid")
            files = ["README"]
            if name == "classic":
                files += ["client/README", "protocol/README", "libatrinik/README",
                          "cmake/AtrinikVersion.cmake", "LICENSE.md", "ATTRIBUTIONS.md",
                          "server/dependencies.lock.json"]
            for filename in files:
                path = root / filename
                path.parent.mkdir(parents=True, exist_ok=True)
                path.write_text('set(ATRINIK_DEVELOPMENT_VERSION "5.1.0")\n'
                                if filename.endswith("AtrinikVersion.cmake") else "qualified source\n")
            git(root, "add", ".")
            git(root, "-c", "commit.gpgsign=false", "commit", "-m", "qualified")
            pins[name] = git(root, "rev-parse", "HEAD").strip()
            (root / "new.txt").write_text("unqualified main advancement\n")
            git(root, "add", ".")
            git(root, "-c", "commit.gpgsign=false", "commit", "-m", "new main")
            tips[name] = git(root, "rev-parse", "HEAD").strip()

        def run(arguments):
            if arguments[0] == "git":
                # Supply fixture fetch bytes, preserving the canonical origin
                # used by the real wrapper's checkout validation.
                if "fetch" in arguments:
                    arguments = [str(origins[arguments[2]]) if arg == "origin" else arg
                                 for arg in arguments]
                return subprocess.run(arguments, check=True, capture_output=True, text=True).stdout
            workspace = Workspace(self.root)
            try:
                if arguments[1] == "init":
                    workspace.initialize(["classic-client", "sound"], jobs=2)
                else:
                    workspace.create_profile(arguments[3], "classic")
            finally:
                workspace.close()
            return ""

        with chdir(self.root), mock.patch.dict(os.environ, {"ATRINIK_WORKSPACE_DIR": str(self.root / "workspace")}), \
             mock.patch.dict(ns["prepare_sources"].__globals__, {"run": run, "SOURCE_COMMITS": pins}):
            profile = ns["prepare_sources"]()
            workspace = Workspace(self.root)
            try:
                # Exercise the same real immutable-generation boundary used by
                # export_client, without invoking a compiler or producer image.
                with workspace._resolved_profile_operation(
                    profile, {"client"}, "portable fixture export", materialize_clean_primaries=True
                ) as snapshot:
                    states = snapshot.checkout_states()
                    for source in snapshot.paths().values():
                        record = workspace._source_generation_record(source)
                        self.assertIsNotNone(record)
                        workspace._validate_source_generation_git_closure(
                            Path(states[record["checkout"]]["path"]), source.parent,
                            record["source_tree"], record["tree"], record["source_includes"])
            finally:
                workspace.close()
        for name, pin in pins.items():
            primary = self.root / name
            self.assertEqual(git(primary, "rev-parse", "HEAD").strip(), pin)
            self.assertEqual(git(primary, "branch", "--show-current").strip(), "main")
            self.assertEqual(git(primary, "remote", "get-url", "origin").strip(),
                             "https://github.com/atrinik/" + name + ".git")
            self.assertEqual(git(primary, "status", "--porcelain"), "")
            self.assertEqual(git(origins[name], "rev-parse", "HEAD").strip(), tips[name])

    def test_acceptance_refuses_existing_dependency_paths_before_mutation(self):
        for kind in ("directory", "file", "dangling-symlink"):
            with self.subTest(kind=kind), tempfile.TemporaryDirectory() as directory, chdir(directory):
                ns = self.acceptance_namespace()
                existing = Path("sound")
                if kind == "directory":
                    existing.mkdir()
                    (existing / "dirty.txt").write_text("preserve")
                elif kind == "file":
                    existing.write_text("preserve")
                else:
                    existing.symlink_to("missing")
                runner = mock.Mock()
                with mock.patch.dict(ns["prepare_sources"].__globals__, {"run": runner}):
                    with self.assertRaisesRegex(RuntimeError, "already exists: sound"):
                        ns["prepare_sources"]()
                runner.assert_not_called()
                self.assertFalse(Path("classic").exists())
                if kind == "directory":
                    self.assertEqual((existing / "dirty.txt").read_text(), "preserve")
                elif kind == "file":
                    self.assertEqual(existing.read_text(), "preserve")
                else:
                    self.assertTrue(existing.is_symlink())

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
            if arguments[1] == "path":
                return "classic" if arguments[2] == "classic-client" else arguments[2]
            if arguments[0] == "git" and "rev-parse" in arguments:
                return ns["SOURCE_COMMITS"].get(arguments[2], "a" * 40) + "\n" if "-C" in arguments else "a" * 40 + "\n"
            return ""
        with mock.patch.dict(globals_, {"require_headless": lambda: None, "run": fake_run,
                              "prepare_sources": lambda: "linux-portable-acceptance",
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


    @unittest.skipUnless(shutil.which("clang") and shutil.which("llvm-readelf"), "Clang and LLVM readelf required")
    def test_other_absolute_runpath_provider_remains_rejected(self):
        from contextlib import ExitStack
        source = self.root / "provider.c"
        library = self.root / "libother.so"
        source.write_text("int other(void) { return 0; }\n")
        subprocess.run(["clang", "-shared", "-fPIC", str(source), "-o", str(library),
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
        subprocess.run(["clang", "-shared", "-fPIC", "-nostdlib", str(source), "-o", str(library),
                        "-Wl,-soname,libfixture.so.1", "-Wl,--version-script=" + str(versions)],
                       check=True, capture_output=True)
        client_source = self.root / "client.c"
        client_source.write_text("extern int sample(void); int main(void) { return sample(); }\n")
        binary = self.root / "client-binary"
        subprocess.run(["clang", "-nostdlib", str(client_source), str(library), "-o", str(binary),
                        "-Wl,-e,main", "-Wl,--dynamic-linker=/lib64/ld-linux-x86-64.so.2"],
                       check=True, capture_output=True)
        documents = {"runtime-abi.json": {"objects": [{
            "path": str(library), "sha256": hashlib.sha256(library.read_bytes()).hexdigest(),
            "needed": [], "dlopen": [{"feature": "fixture", "soname": ["libfixture.so.1"]}],
            "dlopen_providers": {"fixture": [str(library)]},
            "required_providers": {"sample@SAMPLE_1": {"provider": "libfixture.so.1"}},
        }]}, "contract.json": {"runtime": {"unsupported_dlopen_features": []}}}
        return library, binary, documents

    @unittest.skipUnless(shutil.which("clang") and shutil.which("llvm-readelf"), "Clang and LLVM readelf required")
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

    @unittest.skipUnless(shutil.which("clang") and shutil.which("llvm-readelf"), "Clang and LLVM readelf required")
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

    @unittest.skipUnless(shutil.which("clang") and shutil.which("llvm-readelf"), "Clang and LLVM readelf required")
    def test_declared_unsupported_dynamic_feature_is_excluded_explicitly(self):
        library, binary, documents = self.compiled_runtime()
        documents["contract.json"]["runtime"]["unsupported_dlopen_features"] = [
            {"object": str(library), "feature": "fixture", "soname": ["libfixture.so.1"]}]
        with portable.Publication(self.output, self.sources) as publication:
            report = portable.add_runtime(publication, documents, binary)
        self.assertEqual(report["dynamic_sonames"], [])
