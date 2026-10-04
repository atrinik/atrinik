from __future__ import annotations

from contextlib import redirect_stderr, redirect_stdout
import io
import json
from pathlib import Path
import re
import runpy
import subprocess
import sys
import tempfile
import unittest
from unittest import mock

from atrinik_workspace import guidance_inventory
from atrinik_workspace.guidance_inventory import (
    budget_failures,
    collect_inventory,
    file_metrics,
    main,
    render_text,
    skill_frontmatter,
    validate_process_improvement_ledger,
    validate_tooling_ledger,
)


ROOT = Path(__file__).resolve().parents[1]
LINK = re.compile(r"\[[^]]+\]\(([^)]+)\)")


def read_guidance_contract(path: Path) -> str:
    """Read a local guide and the repository-owned references it selects."""
    visited: set[Path] = set()

    def expand(current: Path) -> str:
        current = current.resolve()
        if current in visited:
            return ""
        visited.add(current)
        text = current.read_text(encoding="utf-8")
        references = []
        for target in LINK.findall(text):
            if "://" in target or target.startswith("#"):
                continue
            candidate = (current.parent / target.split("#", 1)[0]).resolve()
            if candidate.is_file() and candidate.is_relative_to(ROOT):
                references.append(expand(candidate))
        return "\n".join((text, *references))

    return expand(path)


class AgentGuidanceTests(unittest.TestCase):
    def test_current_provenance_registry_is_complete(self) -> None:
        registry = " ".join(
            (ROOT / "docs/PROVENANCE.md").read_text(encoding="utf-8").split()
        )
        self.assertIn("Zoey Rose", registry)
        self.assertIn("Daniel Liptrot", registry)
        for marker in {
            "affirmative permissions",
            "used as implementation reference",
            "clean-room isolation is not required",
            "original past Atrinik contributions",
            "not a prospective grant",
            "cannot be combined to cover a jointly authored contribution",
            "not relicensed by these historical grants",
            "does not by itself place agent-generated output",
        }:
            with self.subTest(marker=marker):
                self.assertIn(marker, registry)

        identity_policy = " ".join(
            (ROOT / "docs/PROVENANCE_IDENTITIES.md")
            .read_text(encoding="utf-8")
            .split()
        )
        for marker in {
            "Prior public appearance is not authorization",
            "Removing an `alias` field alone does not prevent re-identification",
            "provenance-custodians",
            "provenance-reviewers",
            "seven years after the last reliance is withdrawn",
            "Suspected disclosure freezes new attestations",
            "Key compromise rotates recipients and HMAC keys",
            "Unkeyed hashes of names, aliases, emails, commits, or ranges are forbidden",
            "full 40-character coordinator commit reachable from canonical `origin/main`",
            "Current named grants in `docs/PROVENANCE.md` are not silently converted",
        }:
            with self.subTest(marker=marker):
                self.assertIn(marker, identity_policy)

        schema = json.loads(
            (ROOT / "governance/provenance-identities/schema-v1.json").read_text(
                encoding="utf-8"
            )
        )
        public_registry = json.loads(
            (ROOT / "governance/provenance-identities/registry.json").read_text(
                encoding="utf-8"
            )
        )
        self.assertEqual(schema["properties"]["schema_version"]["const"], 1)
        self.assertTrue(public_registry["records"])
        self.assertTrue(
            all(record["synthetic"] for record in public_registry["records"])
        )
        fixtures = ROOT / "tests/fixtures/provenance-identities"
        self.assertEqual(len(list((fixtures / "positive").glob("*.json"))), 2)
        self.assertTrue(list((fixtures / "negative").glob("*.json")))

    def test_copyright_header_contract_is_complete(self) -> None:
        guide = " ".join(
            (ROOT / "AGENTS.md").read_text(encoding="utf-8").split()
        )
        contributing = " ".join(
            (ROOT / "CONTRIBUTING.md").read_text(encoding="utf-8").split()
        )

        for marker in {
            "On touch, refresh existing Atrinik-owned copyright terminal years",
            "blanket holders",
            "`CONTRIBUTING.md`",
            "preserve precise attribution",
        }:
            with self.subTest(surface="AGENTS.md", marker=marker):
                self.assertIn(marker, guide)

        for marker in {
            "Use `The Atrinik Project` as the exact collective holder",
            "already predominates in modern MIT source headers",
            "exact blanket format is `Copyright START[-END] The Atrinik Project`",
            "Only the surrounding comment delimiters vary by file format",
            "omit `(C)`, `(c)`, `©`, commas, and trailing punctuation",
            "migrate prospectively",
            "each existing Atrinik-owned copyright notice",
            "retain its original start year",
            "current calendar year",
            "Crossfire, Daimonin and other upstream notices",
            "Leave upstream and third-party notice years unchanged",
            "SPDX identifiers",
            "authoritative generator or template",
            "a separate legal and attribution surface",
        }:
            with self.subTest(surface="CONTRIBUTING.md", marker=marker):
                self.assertIn(marker, contributing)

        for example in {
            "Copyright 2021-2026 The Atrinik Project",
            "Copyright 2026 The Atrinik Project",
            "Copyright 2024-2026 The Atrinik Project",
            "Copyright (C) 2009-2026 Zoey Rose and Atrinik Development Team",
        }:
            with self.subTest(example=example):
                self.assertIn(example, contributing)

    def test_inventory_is_complete_and_within_budget(self) -> None:
        inventory = collect_inventory()
        self.assertEqual(inventory["external_metrics"], "unmeasured")
        self.assertIsNone(inventory["summary"]["skill_count"])
        self.assertEqual(len(inventory["provider"]["required_skills"]), 12)
        self.assertEqual(inventory["skills"], [])
        self.assertEqual(budget_failures(inventory), [])
        self.assertIn("external-metrics\tunmeasured", render_text(inventory))
        metrics = file_metrics(ROOT / "AGENTS.md")
        self.assertEqual(metrics.path, "AGENTS.md")
        self.assertGreater(metrics.bytes, 0)

    def test_frontmatter_validation_fails_closed(self) -> None:
        cases = {
            "missing": "# no frontmatter\n",
            "unterminated": "---\nname: skill\n",
            "unsupported": "---\nname: skill\nsummary: no\n---\n",
            "duplicate": (
                "---\nname: skill\nname: skill\ndescription: duplicate\n---\n"
            ),
            "incomplete": "---\nname: skill\n---\n",
            "mismatched": "---\nname: other\ndescription: mismatch\n---\n",
        }
        with tempfile.TemporaryDirectory() as temporary:
            path = Path(temporary) / "skill" / "SKILL.md"
            path.parent.mkdir()
            for name, content in cases.items():
                with self.subTest(name=name):
                    path.write_text(content, encoding="utf-8")
                    with self.assertRaises(ValueError):
                        skill_frontmatter(path)

    def test_tooling_ledger_is_optional_ignored_and_secret_safe(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            subprocess.run(
                ['git', 'init', '--quiet'],
                cwd=root,
                check=True,
                stdout=subprocess.DEVNULL,
                stderr=subprocess.DEVNULL,
            )
            ignore = root / '.gitignore'
            ignore.write_text('/build/\n', encoding='utf-8')
            ledger = root / 'build/agent-tooling-issues.md'
            self.assertEqual(validate_tooling_ledger(root), [])

            ledger.parent.mkdir()
            valid = (
                '# Agent tooling issues\n\n'
                '| Stable key | Status | Observation | Impact | Recommended action |\n'
                '| --- | --- | --- | --- | --- |\n'
                '| `mechanism=docker-mount;remediation=canonical-container` | open | '
                'generic mount symptom | delivery was delayed | use the pinned container |\n'
            )
            ledger.write_text(valid, encoding='utf-8')
            self.assertEqual(validate_tooling_ledger(root), [])

            cases = {
                'invalid stable key': valid.replace(
                    'mechanism=docker-mount;remediation=canonical-container',
                    'docker-mount',
                ),
                'secret-like value': valid.replace(
                    'generic mount symptom', 'token: example'
                ),
                'private host path': valid.replace(
                    'generic mount symptom', r'C:\Users\operator\mount.log'
                ),
            }
            cases['duplicate stable key'] = valid + (
                '| `mechanism=docker-mount;remediation=canonical-container` | open | '
                'same symptom | same impact | same action |\n'
            )
            for name, content in cases.items():
                with self.subTest(name=name):
                    ledger.write_text(content, encoding='utf-8')
                    self.assertTrue(validate_tooling_ledger(root), name)

            ignore.write_text('/other/\n', encoding='utf-8')
            ledger.write_text(valid, encoding='utf-8')
            self.assertTrue(
                any(
                    'is not ignored' in failure
                    for failure in validate_tooling_ledger(root)
                )
            )

            ignore.write_text('/build/\n', encoding='utf-8')
            cases = {
                'binary data': (
                    valid.replace('generic mount symptom', 'generic\x00 mount symptom'),
                    'contains binary data',
                ),
                'secret-like header': (
                    (
                        '| Token | Status | Observation | Impact | Recommended action |\n'
                        '| --- | --- | --- | --- | --- |\n'
                    )
                    + valid,
                    'contains a secret-like field',
                ),
                'missing table': ('# notes\n', 'missing the required Markdown table'),
                'invalid separator': (
                    valid.replace(
                        '| --- | --- | --- | --- | --- |',
                        '| -- | --- | --- | --- | --- |',
                    ),
                    'invalid Markdown table separator',
                ),
                'missing separator': (
                    '| Stable key | Status | Observation | Impact | Recommended action |\n',
                    'invalid Markdown table separator',
                ),
                'malformed row': (valid + '| malformed\n', 'malformed table row'),
                'wrong width row': (
                    valid + '| `mechanism=bad;remediation=width` | open | impact |\n',
                    'malformed table row',
                ),
                'empty field': (
                    valid
                    + '| `mechanism=empty;remediation=field` | open |  | impact | action |\n',
                    'empty required field',
                ),
                'invalid status': (
                    valid
                    + '| `mechanism=bad;remediation=status` | pending | observation | impact | action |\n',
                    'invalid status',
                ),
            }
            for name, (content, expected) in cases.items():
                with self.subTest(name=name):
                    ledger.write_text(content, encoding='utf-8')
                    self.assertTrue(
                        any(expected in failure for failure in validate_tooling_ledger(root))
                    )

            ledger.write_text(valid + 'trailing prose\n', encoding='utf-8')
            self.assertEqual(validate_tooling_ledger(root), [])

            ledger.write_bytes(b'\xff')
            self.assertTrue(
                any('is not UTF-8' in failure for failure in validate_tooling_ledger(root))
            )

            ledger.write_bytes(
                b'x' * (guidance_inventory.TOOLING_LEDGER_MAX_BYTES + 1)
            )
            self.assertTrue(
                any('exceeds the size limit' in failure for failure in validate_tooling_ledger(root))
            )

            ledger.write_text(valid, encoding='utf-8')
            with mock.patch.object(Path, 'read_bytes', side_effect=OSError):
                self.assertTrue(
                    any('could not be read' in failure for failure in validate_tooling_ledger(root))
                )

            with mock.patch.object(Path, 'is_symlink', return_value=True):
                self.assertTrue(
                    any('not a regular file' in failure for failure in validate_tooling_ledger(root))
                )
            with mock.patch.object(Path, 'is_file', return_value=False):
                self.assertTrue(
                    any('not a regular file' in failure for failure in validate_tooling_ledger(root))
                )

            subprocess.run(
                ['git', 'add', '--force', 'build/agent-tooling-issues.md'],
                cwd=root,
                check=True,
                stdout=subprocess.DEVNULL,
                stderr=subprocess.DEVNULL,
            )
            self.assertTrue(
                any('is tracked' in failure for failure in validate_tooling_ledger(root))
            )

        with tempfile.TemporaryDirectory() as temporary:
            with mock.patch.object(
                guidance_inventory.subprocess, 'run', side_effect=OSError
            ):
                failures = validate_tooling_ledger(Path(temporary))
            self.assertEqual(
                failures,
                ['build/agent-tooling-issues.md is not ignored'],
            )

    def test_optional_ledger_diagnostics_do_not_fail_guidance_check(self) -> None:
        stderr = io.StringIO()
        with mock.patch.object(
            guidance_inventory, "validate_tooling_ledger", return_value=["ledger failure"]
        ), mock.patch.object(
            guidance_inventory, "validate_process_improvement_ledger",
            return_value=["invalid process ledger"],
        ), redirect_stdout(io.StringIO()), redirect_stderr(stderr):
            self.assertEqual(main(["--check", "--diagnose-ledgers"]), 0)
        self.assertIn("guidance tooling ledger diagnostic: ledger failure", stderr.getvalue())
        self.assertIn("guidance process ledger diagnostic: invalid process ledger", stderr.getvalue())

    def test_optional_ledger_inspection_errors_are_nonblocking_and_redacted(self) -> None:
        for exception in (OSError, UnicodeError, ValueError, subprocess.TimeoutExpired):
            with self.subTest(exception=exception.__name__):
                error = (exception("private detail", 5) if exception is subprocess.TimeoutExpired
                         else exception("private detail"))
                stderr = io.StringIO()
                with mock.patch.object(
                    guidance_inventory, "validate_tooling_ledger", side_effect=error
                ), mock.patch.object(
                    guidance_inventory, "validate_process_improvement_ledger", side_effect=error
                ), redirect_stdout(io.StringIO()), redirect_stderr(stderr):
                    self.assertEqual(main(["--check", "--diagnose-ledgers"]), 0)
                self.assertEqual(stderr.getvalue().count("could not inspect optional ledger"), 2)
                self.assertNotIn("private detail", stderr.getvalue())

    def test_explicit_provider_requires_every_wrapper_skill(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            with self.assertRaisesRegex(ValueError, "missing required provider skill"):
                collect_inventory(Path(temporary))

    def test_process_improvement_ledger_is_optional_and_fail_closed(self) -> None:
        valid = """# Agent process improvements

| Key | Status | Observation | Expected benefit / proposed action | Related issue / PR | Last observed (UTC) |
| --- | --- | --- | --- | --- | --- |
| `cache-reuse` | observed | A cache can be reused. | Keep the cache warm. | none | 2026-09-02T00:00:00Z |
"""
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            self.assertEqual(validate_process_improvement_ledger(root), [])

            ledger = root / guidance_inventory.PROCESS_IMPROVEMENT_LEDGER
            ledger.parent.mkdir()
            ledger.write_text(valid, encoding="utf-8")
            with mock.patch.object(
                guidance_inventory.subprocess,
                "run",
                return_value=mock.Mock(returncode=0),
            ) as check_ignore:
                self.assertEqual(validate_process_improvement_ledger(root), [])
            check_ignore.assert_called_once_with(
                [
                    "git",
                    "check-ignore",
                    "--no-index",
                    "--quiet",
                    "--",
                    "build/agent-process-improvements.md",
                ],
                cwd=root,
                capture_output=True,
                check=False,
                timeout=5,
            )

            cases = {
                "duplicate stable keys": valid.replace(
                    "| `cache-reuse` | observed | A cache can be reused. | Keep the cache warm. | none | 2026-09-02T00:00:00Z |",
                    "| `cache-reuse` | observed | A cache can be reused. | Keep the cache warm. | none | 2026-09-02T00:00:00Z |\n| `cache-reuse` | observed | A second observation. | Keep the cache warm. | none | 2026-09-02T00:00:00Z |",
                ),
                "invalid status": valid.replace("| observed |", "| complete |"),
                "missing field": valid.replace("| Keep the cache warm. |", "|  |"),
                "secret-like content": valid.replace(
                    "A cache can be reused.", "token: do-not-store-this"
                ),
                "private host content": valid.replace(
                    "A cache can be reused.", "host: internal.example"
                ),
                "invalid timestamp": valid.replace(
                    "2026-09-02T00:00:00Z", "2026-99-99T00:00:00Z"
                ),
            }
            expected_errors = {
                "duplicate stable keys": "duplicate stable keys",
                "invalid status": "invalid status",
                "missing field": "all descriptive fields",
                "secret-like content": "secret-like content",
                "private host content": "secret-like content",
                "invalid timestamp": "UTC last-observed timestamps",
            }
            for name, content in cases.items():
                with self.subTest(name=name):
                    ledger.write_text(content, encoding="utf-8")
                    with mock.patch.object(
                        guidance_inventory.subprocess,
                        "run",
                        return_value=mock.Mock(returncode=0),
                    ):
                        errors = validate_process_improvement_ledger(root)
                    self.assertTrue(errors)
                    self.assertTrue(
                        any(expected_errors[name] in error for error in errors),
                        errors,
                    )

            ledger.write_text(valid, encoding="utf-8")
            with mock.patch.object(
                guidance_inventory.subprocess,
                "run",
                return_value=mock.Mock(returncode=1),
            ):
                errors = validate_process_improvement_ledger(root)
            self.assertIn("not ignored", " ".join(errors))

    def test_process_improvement_ledger_rejects_structural_and_io_failures(self) -> None:
        valid = """# Agent process improvements

| Key | Status | Observation | Expected benefit / proposed action | Related issue / PR | Last observed (UTC) |
| --- | --- | --- | --- | --- | --- |
| `cache-reuse` | observed | A cache can be reused. | Keep the cache warm. | none | 2026-09-02T00:00:00Z |
"""
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            ledger = root / guidance_inventory.PROCESS_IMPROVEMENT_LEDGER
            ledger.parent.mkdir()

            def validate(content: str | bytes, **patch_kwargs: object) -> list[str]:
                if isinstance(content, bytes):
                    ledger.write_bytes(content)
                else:
                    ledger.write_text(content, encoding="utf-8")
                with mock.patch.object(
                    guidance_inventory.subprocess,
                    "run",
                    **{"return_value": mock.Mock(returncode=0), **patch_kwargs},
                ):
                    return validate_process_improvement_ledger(root)

            self.assertIn(
                "cannot verify",
                " ".join(
                    validate(valid, side_effect=OSError("git unavailable"))
                ),
            )
            self.assertIn(
                "cannot verify",
                " ".join(
                    validate(
                        valid,
                        side_effect=subprocess.TimeoutExpired(["git"], 5),
                    )
                ),
            )
            self.assertIn(
                "bounded UTF-8",
                " ".join(validate(b"\xff")),
            )
            self.assertIn(
                "exceeds 128 KiB",
                " ".join(validate(valid + "x" * (128 * 1024))),
            )

            cases = {
                "missing newline": valid.rstrip("\n"),
                "control character": valid.replace("A cache", "A \x01cache"),
                "wrong title": valid.replace(
                    "# Agent process improvements", "# Other improvements"
                ),
                "missing table": "# Agent process improvements\n\nNo rows yet.\n",
                "missing separator": (
                    "# Agent process improvements\n\n"
                    + guidance_inventory.PROCESS_TABLE_HEADER
                    + "\n"
                ),
                "invalid separator": valid.replace(
                    "| --- | --- | --- | --- | --- | --- |",
                    "| - | --- | --- | --- | --- | --- |",
                ),
                "wrong row shape": valid.replace(
                    "| `cache-reuse` | observed | A cache can be reused. | "
                    "Keep the cache warm. | none | 2026-09-02T00:00:00Z |",
                    "| `cache-reuse` | observed | A cache can be reused. | "
                    "Keep the cache warm. | none |",
                ),
                "invalid stable key": valid.replace(
                    "`cache-reuse`", "`Cache reuse`"
                ),
                "invalid timestamp shape": valid.replace(
                    "2026-09-02T00:00:00Z", "not-a-timestamp"
                ),
                "empty ledger": valid.split("| `cache-reuse`", 1)[0],
                "blank before row": valid.replace(
                    "| `cache-reuse`", "\n| `cache-reuse`"
                ),
                "text before row": valid.replace(
                    "| `cache-reuse`", "note\n| `cache-reuse`"
                ),
                "blank after row": valid + "\n",
                "text after row": valid + "notes\n",
            }
            expected_errors = {
                "missing newline": "end with a newline",
                "control character": "control characters",
                "wrong title": "canonical title",
                "missing table": "canonical table",
                "missing separator": "requires a separator",
                "invalid separator": "separator is invalid",
                "wrong row shape": "six fields",
                "invalid stable key": "invalid stable key",
                "invalid timestamp shape": "UTC last-observed timestamps",
                "empty ledger": "at least one row",
            }
            for name, content in cases.items():
                with self.subTest(name=name):
                    errors = validate(content)
                    if name in expected_errors:
                        self.assertIn(expected_errors[name], " ".join(errors))
                    else:
                        self.assertEqual(errors, [])

            directory_root = root / "directory"
            directory_root.mkdir()
            directory_ledger = directory_root / guidance_inventory.PROCESS_IMPROVEMENT_LEDGER
            directory_ledger.mkdir(parents=True)
            self.assertIn(
                "regular file",
                " ".join(validate_process_improvement_ledger(directory_root)),
            )

            symlink_path = root / "symlink" / guidance_inventory.PROCESS_IMPROVEMENT_LEDGER
            symlink_path.parent.mkdir(parents=True)
            with mock.patch.object(
                type(symlink_path), "is_symlink", return_value=True
            ):
                self.assertIn(
                    "must not be a symlink",
                    " ".join(validate_process_improvement_ledger(root / "symlink")),
                )

    def test_inventory_skips_optional_ledgers_and_script_entrypoint_runs(self) -> None:
        with mock.patch.object(
            guidance_inventory, "validate_process_improvement_ledger",
            side_effect=AssertionError("optional process ledger was inspected"),
        ) as process_validator, mock.patch.object(
            guidance_inventory, "validate_tooling_ledger",
            side_effect=AssertionError("optional tooling ledger was inspected"),
        ) as tooling_validator, redirect_stdout(io.StringIO()):
            collect_inventory()
            self.assertEqual(main(["--check"]), 0)
            process_validator.assert_not_called()
            tooling_validator.assert_not_called()

        with self.assertRaises(SystemExit) as exit_info:
            with mock.patch.object(sys, "argv", ["guidance_inventory.py"]):
                with redirect_stdout(io.StringIO()):
                    runpy.run_path(
                        str(ROOT / "atrinik_workspace/guidance_inventory.py"),
                        run_name="__main__",
                    )
        self.assertEqual(exit_info.exception.code, 0)

    def test_inaccessible_optional_ledger_metadata_does_not_fail_check(self) -> None:
        path = mock.Mock()
        path.exists.side_effect = PermissionError("private path detail")
        stdout = io.StringIO()
        stderr = io.StringIO()
        with mock.patch.object(
            guidance_inventory, "process_improvement_ledger_path", return_value=path
        ), redirect_stdout(stdout), redirect_stderr(stderr):
            self.assertEqual(main(["--check", "--json"]), 0)
        self.assertIsNone(json.loads(stdout.getvalue())["process_improvements"]["present"])
        self.assertEqual(stderr.getvalue(), "")

    def test_command_output_and_failures(self) -> None:
        stdout = io.StringIO()
        with redirect_stdout(stdout):
            self.assertEqual(main([]), 0)
        self.assertIn("summary\tcatalog=", stdout.getvalue())

        stdout = io.StringIO()
        with redirect_stdout(stdout):
            self.assertEqual(main(["--json"]), 0)
        inventory = json.loads(stdout.getvalue())
        self.assertIsNone(inventory["summary"]["skill_count"])
        self.assertEqual(
            inventory["process_improvements"]["path"],
            "build/agent-process-improvements.md",
        )
        self.assertIsInstance(inventory["process_improvements"]["present"], bool)

        stderr = io.StringIO()
        with mock.patch.object(
            guidance_inventory, "budget_failures", return_value=["test ceiling"]
        ), redirect_stdout(io.StringIO()), redirect_stderr(stderr):
            self.assertEqual(main(["--check"]), 1)
        self.assertIn("guidance budget failed: test ceiling", stderr.getvalue())

        stderr = io.StringIO()
        with mock.patch.object(
            guidance_inventory,
            "collect_inventory",
            side_effect=ValueError("invalid guidance"),
        ), redirect_stderr(stderr):
            self.assertEqual(main([]), 1)
        self.assertIn(
            "guidance inventory failed: invalid guidance", stderr.getvalue()
        )

    def test_optional_process_diagnostics_are_guided(self) -> None:
        for path in (
            ROOT / "AGENTS.md",
        ):
            with self.subTest(path=path):
                text = " ".join(path.read_text(encoding="utf-8").split()).lower()
                self.assertRegex(text, r"optional|discretionary")
                self.assertIn("./atrinik agent-ledger update", text)
                self.assertNotIn("process improvements added: none", text)
                self.assertNotIn("tooling issues: none", text)

    def test_native_development_guidance_preserves_authority_boundaries(self) -> None:
        paths = [
            ROOT / "AGENTS.md",
            ROOT / "README.md",
            ROOT / "docs/ARCHITECTURE.md",
        ]
        for path in paths:
            text = read_guidance_contract(path)
            normalized = " ".join(text.split())
            with self.subTest(path=path.relative_to(ROOT)):
                self.assertIn("native", normalized.lower())
                self.assertIn("worktree", normalized)
                self.assertIn("pinned", normalized)
                self.assertIn("ledger", normalized)
                self.assertIn("leases", normalized)
                self.assertRegex(
                    normalized,
                    r"Codex (?:never|must never) (?:launches|launch|launch or controls)"
                    r"[^.]{0,100}VS Code",
                )
                self.assertIn("GUI automation", normalized)
        execution = (ROOT / "docs/LINUX_EXECUTION.md").read_text()
        compatibility = execution.split("## Existing bound container compatibility", 1)[1].split("\n## ", 1)[0]
        for gate in ("authenticated actor", "complete inventory", "clean",
                     "public CAS", "ordered leases", "30 minutes/12 hours",
                     "replace, remount, transfer or adopt"):
            self.assertIn(gate, " ".join(compatibility.split()))

    def test_native_windows_gpu_handoff_is_synchronized(self) -> None:
        handoff = ROOT / "docs/WINDOWS_GPU_PREFLIGHT.md"
        self.assertTrue(handoff.is_file())
        handoff_text = handoff.read_text(encoding="utf-8")
        for marker in {
            "native-windows-classic-gpu-preflight",
            "native-package-smoke",
            "d3d12-benchmark",
            "linux-only-coordinator",
            "ATRINIK_GPU_CONFORMANCE_DRIVER",
            "scripts/validate_windows_gpu_evidence.py",
        }:
            with self.subTest(marker=marker):
                self.assertIn(marker, handoff_text)

        for path in {
            ROOT / "AGENTS.md",
            ROOT / "README.md",
            ROOT / "docs/ARCHITECTURE.md",
        }:
            with self.subTest(path=path.relative_to(ROOT)):
                self.assertIn("WINDOWS_GPU_PREFLIGHT.md", path.read_text(encoding="utf-8"))

    def test_container_launch_recipes_are_build_or_runtime_scoped(self) -> None:
        paths = [ROOT / "README.md", ROOT / "AGENTS.md", ROOT / "CONTRIBUTING.md"]
        paths.extend(sorted((ROOT / "docs").glob("*.md")))
        allowed_configs = (
            ".devcontainer/server-runtime.json",
            ".devcontainer/windows-cross/devcontainer.json",
        )
        for path in paths:
            # Qualification is immutable historical evidence, not a current recipe.
            if path.name == "NATIVE_LINUX_QUALIFICATION.md":
                continue
            text = path.read_text(encoding="utf-8")
            blocks = re.findall(r"(?:```|~~~)[^\n]*\n(.*?)(?:```|~~~)", text, re.DOTALL)
            for block in blocks:
                commands = block.replace("\\\n", " ")
                for line in commands.splitlines():
                    if re.match(r"\s*devcontainer\s+(?:up|exec)\b", line):
                        with self.subTest(path=path.relative_to(ROOT), command=line):
                            self.assertTrue(any(config in line for config in allowed_configs))
                    with self.subTest(path=path.relative_to(ROOT), command=line):
                        self.assertNotRegex(line, r"^\s*(?:code(?:\.cmd)?\b|vscode://|(?:xdotool|ydotool|osascript|wmctrl)\b)")
            with self.subTest(path=path.relative_to(ROOT)):
                self.assertNotIn("Dev Containers: Reopen in Container", text)
                self.assertNotIn("Dev Containers: Rebuild Container", text)
        auth = (ROOT / "docs/COORDINATOR_AUTH.md").read_text()
        self.assertNotRegex(auth, r"gh auth token[^\n]*\|")

    def test_native_delivery_is_independent_of_build_worker_lifetime(self) -> None:
        paths = (
            "AGENTS.md", "README.md", "docs/ARCHITECTURE.md",
            "docs/LINUX_EXECUTION.md", "docs/COORDINATOR_AUTH.md",
            "docs/PROJECT_DELIVERY_GOAL.md",
        )
        for path in paths:
            text = " ".join((ROOT / path).read_text(encoding="utf-8").split())
            with self.subTest(path=path):
                self.assertIn("native", text.lower())
                self.assertIn("worktree", text)
                self.assertIn("container", text)
                self.assertNotIn("A session is one agent-owned container", text)
        execution = (ROOT / "docs/LINUX_EXECUTION.md").read_text(encoding="utf-8")
        for invariant in ("same-absolute-path", "--pull never", "--expected-plan",
                          "--git-common-dir", "atrinik-resource-leases", "--cap-drop ALL",
                          "unchanged path and inode", "No mandatory host compiler", "cache keys",
                          "Distinct concurrent owners", "not a drop-in credential-free worker",
                          "no dirty-work adoption or ledger rewriting"):
            self.assertIn(invariant, execution)
        # The concrete worker arguments expose neither coordinator credentials
        # nor a Docker/display/GPU socket. Historical bound deliveries retain
        # their separate authentication and compatibility gates.
        arguments = execution.split("BUILD_ARGS=(", 1)[1].split("\n# Create, inspect", 1)[0]
        for forbidden in (".config/gh", ".codex", "docker.sock", "--privileged", "--gpus"):
            self.assertNotIn(forbidden, arguments)
        self.assertNotIn("mode=000", arguments)
        self.assertIn("/tmp:rw,exec,nosuid,nodev,mode=1777", arguments)
        self.assertIn("target=$REVIEW_ROOT,readonly", arguments)
        self.assertIn("never substitutes", " ".join(execution.split()))
        for required in ("$NATIVE_PRIMARY", "$NATIVE_WORKTREE", "$COMMON_GIT", "$BUILD_LEASES", "$REVIEW_ROOT"):
            self.assertIn(required, arguments)

    def test_local_guidance_links_resolve(self) -> None:
        paths = [ROOT / "AGENTS.md", ROOT / "docs/SKILL_PROVIDER.md"]
        for path in paths:
            for target in LINK.findall(path.read_text(encoding="utf-8")):
                if "://" in target or target.startswith("#"):
                    continue
                with self.subTest(path=path.relative_to(ROOT), target=target):
                    resolved = (path.parent / target.split("#", 1)[0]).resolve()
                    self.assertTrue(resolved.is_relative_to(ROOT))
                    self.assertTrue(resolved.is_file())

    def test_issue_delivery_provisioning_examples_match_manifest(self) -> None:
        examples = json.loads((ROOT / "tests/fixtures/delivery-guidance-examples.json").read_text())
        checkouts = {
            checkout["name"]: checkout["repository"].split("/", 1)
            for checkout in json.loads(
                (ROOT / "components.json").read_text(encoding="utf-8")
            )["checkouts"]
        }

        pr_ledger = next(
            example
            for example in examples
            if isinstance(example, dict) and example.get("entry_mode") == "pr"
        )
        primitive = next(
            artifact["primitive_request"]
            for artifact in pr_ledger["artifacts"]
            if artifact.get("primitive_request") is not None
        )
        owner, repository = checkouts[primitive["physical_checkout"]]
        self.assertEqual(
            primitive["repository"],
            {"owner": owner, "name": repository, "node_id": "R_repo"},
        )
        self.assertTrue(
            all(
                target["repository"]["owner"] == owner
                and target["repository"]["name"] == repository
                for target in pr_ledger["targets"]
            )
        )

        issue_ledger = next(
            example
            for example in examples
            if isinstance(example, dict) and example.get("entry_mode") == "issue"
        )
        issue_worktree = next(
            artifact
            for artifact in issue_ledger["artifacts"]
            if artifact["kind"] == "worktree"
        )
        self.assertIsNone(issue_worktree["immutable"]["path"])
        self.assertIsNone(issue_worktree["producer_resource_slot"])
        issue_request = issue_worktree["primitive_request"]
        self.assertEqual(issue_request["component"], "atrinik")
        self.assertEqual(issue_request["physical_checkout"], "atrinik")
        self.assertEqual(
            issue_request["repository"],
            {"owner": "atrinik", "name": "atrinik", "node_id": "R_repo"},
        )

        scope = next(example[0] for example in examples if isinstance(example, list))
        owner, repository = checkouts[scope["request"]["physical_checkout"]]
        self.assertEqual(scope["immutable"]["repository"]["owner"], owner)
        self.assertEqual(scope["immutable"]["repository"]["name"], repository)


    def test_issue_authoring_contract_names_supported_content_path(self) -> None:
        root_guide = " ".join(
            (ROOT / "AGENTS.md").read_text(encoding="utf-8").split()
        )
        contributing = " ".join(
            (ROOT / "CONTRIBUTING.md").read_text(encoding="utf-8").split()
        )
        for surface in (root_guide, contributing):
            for marker in {
                "content@main",
                "Classic-target artifact",
                "historical evidence",
                "release label",
                "backport destination",
            }:
                with self.subTest(surface=surface[:24], marker=marker):
                    self.assertIn(marker, surface)
        self.assertIn("python3 -m atrinik_workspace.issue_contract PATH", contributing)


    def test_removed_stale_routes_do_not_return(self) -> None:
        paths = [ROOT / "AGENTS.md"]
        corpus = "\n".join(path.read_text(encoding="utf-8") for path in paths)
        for stale in {
            "mixed-component profile",
            "more than one standalone repository",
            "Today this seed repository",
            "scenario create NAME --state",
        }:
            with self.subTest(stale=stale):
                self.assertNotIn(stale, corpus)

    def test_ssh_signing_guidance_is_optional_and_secret_safe(self) -> None:
        contributing = (ROOT / "CONTRIBUTING.md").read_text(encoding="utf-8")
        agents = (ROOT / "AGENTS.md").read_text(encoding="utf-8")
        corpus = "\n".join((contributing, agents))
        normalized = " ".join(corpus.split())

        for marker in {
            "SSH commit signing is optional",
            "Git 2.34",
            "Ed25519",
            "ssh-agent",
            "gpg.format ssh",
            "user.signingkey",
            "commit.gpgsign true",
            "gpgsig",
            "**Verified**",
            "Signed-off-by",
            "Build workers receive no signing agent or credentials",
            "public key",
            "private key",
            "verified author email",
            "SSH signing reference",
        }:
            with self.subTest(marker=marker):
                self.assertIn(marker, normalized)

        self.assertNotRegex(
            corpus, r"-----BEGIN [^-]*PRIVATE KEY-----"
        )
        self.assertNotRegex(
            corpus,
            r"(?im)^\s*(?:all|every|each)\s+commits?\s+must\s+be\s+signed\b",
        )
        self.assertNotRegex(
            corpus,
            r"(?i)\b(?:signed commits|commit signing|commit signatures)\s+"
            r"(?:is|are)\s+(?:required|mandatory)\b",
        )
        self.assertNotRegex(
            corpus,
            r"(?i)\b(?:repository|project|organization)\s+"
            r"(?:requires|enforces|mandates)\s+"
            r"(?:signed commits|commit signing)\b",
        )


    def test_pull_request_publication_contract_is_synchronized(self) -> None:
        root_guide = ROOT / "AGENTS.md"
        contributing = ROOT / "CONTRIBUTING.md"
        governed = [contributing]
        markers = {
            "type(optional-scope): concise description",
            "reviewer explicitly requests a breaking change",
            "GitHub-Flavored Markdown",
            "actual line breaks",
            "literal `\\n` separators",
            "multi-section",
        }
        substantive_markers = {
            "PR bodies must be substantive",
            "`Summary`",
            "`Implementation / behavior`",
            "`Validation`",
            "`Limitations / follow-up`",
            "issue-closing line alone is insufficient",
            "preserve contributor-authored text",
            "byte-for-byte",
            "delivery-owned section",
        }
        for path in governed:
            guidance = " ".join(path.read_text(encoding="utf-8").split())
            for marker in markers:
                with self.subTest(path=path.relative_to(ROOT), marker=marker):
                    self.assertIn(marker, guidance)
            for marker in substantive_markers:
                with self.subTest(
                    path=path.relative_to(ROOT), marker=marker
                ):
                    self.assertIn(marker, guidance)
            with self.subTest(path=path.relative_to(ROOT), marker="no automatic !"):
                self.assertNotIn(
                    "type(optional-scope)!: concise description", guidance
                )
            with self.subTest(path=path.relative_to(ROOT), marker="body input"):
                self.assertRegex(
                    guidance,
                    r"multi-section bod(?:y|ies)[^.]{0,200}file"
                    r"[^.]{0,200}(?:standard input|stdin)",
                )
            with self.subTest(path=path.relative_to(ROOT), marker="remote render"):
                self.assertRegex(
                    guidance,
                    r"[Aa]fter (?:create/edit|creating or editing a pull request)"
                    r"[^.]{0,160}(?:inspect|verify)[^.]{0,80}(?:remote|GitHub)",
                )

        for path in [contributing]:
            guidance = " ".join(path.read_text(encoding="utf-8").split())
            for marker in {
                "headings",
                "lists",
                "inline code",
                "issue-closing references",
                "validation sections",
                "bodyHTML",
                "raw body",
            }:
                with self.subTest(path=path.relative_to(ROOT), marker=marker):
                    self.assertIn(marker, guidance)

        for path in [root_guide]:
            with self.subTest(path=path.relative_to(ROOT), route="governance"):
                self.assertIn("atrinik-github-governance", path.read_text(encoding="utf-8"))

        title_workflow = (ROOT / ".github/workflows/pr-title.yml").read_text(
            encoding="utf-8"
        )
        self.assertIn("pull_request_target:", title_workflow)
        self.assertIn(
            "types: [opened, edited, synchronize, reopened]", title_workflow
        )
        self.assertIn("name: Conventional PR title", title_workflow)
        self.assertIn("type(optional-scope): concise description", title_workflow)
        self.assertIn(
            "add ! only for an explicitly requested breaking change", title_workflow
        )

        run_match = re.search(
            r"(?m)^ {8}run: \|\n(?P<script>(?:^ {10}.*(?:\n|$))+)",
            title_workflow,
        )
        if run_match is None:
            self.fail("PR title workflow does not declare its validation script")
        validation_script = "\n".join(
            line[10:] for line in run_match.group("script").splitlines()
        )
        title_cases = {
            True: {
                "chore: refresh guidance",
                "docs(agents): govern PR publication",
                "feat!: revise the contract",
            },
            False: {
                "Docs: uppercase type",
                "docs(): empty scope",
                "docs: ",
                "update guidance",
            },
        }
        for should_match, titles in title_cases.items():
            for title in titles:
                with self.subTest(title=title, should_match=should_match):
                    result = subprocess.run(
                        ["bash", "-c", validation_script],
                        check=False,
                        capture_output=True,
                        env={"PR_TITLE": title},
                        text=True,
                    )
                    self.assertEqual(
                        result.returncode == 0,
                        should_match,
                        result.stderr,
                    )

    def test_issue_delivery_routes_new_and_retained_work_to_canonical_contracts(self) -> None:
        source = (ROOT / "docs/SOURCE_DELIVERY.md").read_text(encoding="utf-8")
        for invariant in {
            "it is not a delivery ledger",
            "--allow-dirty",
            "fixture tests cannot authorize live resources",
            "Missing acceptance is not a pass",
            "--expect",
        }:
            self.assertIn(invariant, source)


if __name__ == "__main__":
    unittest.main()
