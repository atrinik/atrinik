from __future__ import annotations

import argparse
from contextlib import ExitStack, redirect_stderr, redirect_stdout
import io
import json
import os
from pathlib import Path
import shutil
import shlex
import subprocess
import sys
import tempfile
import unittest
from unittest import mock

from atrinik_workspace.cli import main, parser
from atrinik_workspace import completion
from atrinik_workspace.process_tree import control_socket_path
from atrinik_workspace.completion import classified_actions, complete, shell_script
from atrinik_workspace.model import Manifest


ROOT = Path(__file__).resolve().parents[1]


class CompletionTests(unittest.TestCase):
    def setUp(self) -> None:
        self.temporary = tempfile.TemporaryDirectory()
        self.root = Path(self.temporary.name)
        self.wrapper = self.root / "wrapper"
        self.wrapper.mkdir()
        shutil.copy2(ROOT / "components.json", self.wrapper / "components.json")
        self.workspace = self.root / "state with spaces"
        self.environment = mock.patch.dict(
            os.environ, {"ATRINIK_WORKSPACE_DIR": str(self.workspace)}
        )
        self.environment.start()
        self.workspace.mkdir()
        (self.workspace / ".atrinik-workspace.json").write_text(
            json.dumps({"schema_version": 1}), encoding="utf-8"
        )

    def tearDown(self) -> None:
        self.environment.stop()
        self.temporary.cleanup()

    def candidates(self, *words: str) -> tuple[str, list[str]]:
        values = ["atrinik", *words]
        return complete(parser(), self.wrapper, values, len(values) - 1)

    def write_json(self, path: Path, value: object) -> None:
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(json.dumps(value), encoding="utf-8")

    def profile_record(self, name: str, stack: str) -> dict[str, object]:
        manifest = Manifest.load(self.wrapper / "components.json")
        return {
            "schema_version": 3,
            "name": name,
            "stack": stack,
            "components": {
                component.name: {"kind": "primary", "value": ""}
                for component in manifest.stacks[stack].components
            },
        }

    def scenario_record(self, name: str) -> dict[str, object]:
        manifest = Manifest.load(self.wrapper / "components.json")
        stack = manifest.stacks["classic"]
        roles = {"server", "content", "resources", "libatrinik", "protocol"}
        providers = {role: stack.providers[role].name for role in sorted(roles)}
        resolved = {}
        for role in sorted(roles):
            provider = stack.providers[role]
            checkout_path = self.root / provider.checkout_name
            resolved[role] = {
                "path": str(checkout_path / provider.source),
                "checkout_path": str(checkout_path),
                "checkout": provider.checkout_name,
                "repository": provider.repository,
                "branch": provider.branch,
                "source": provider.source,
                "head": "a" * 40,
                "dirty": False,
            }
        return {
            "schema_version": 4,
            "name": name,
            "profile": "classic",
            "stack": "classic",
            "providers": providers,
            "preset": "basic-player",
            "state": f"scenario-{name}",
            "account": "scenario292",
            "character": "Scenario 292",
            "archetype": "human_male",
            "resolved": resolved,
            "provisioned_at": "2026-08-10T00:00:00Z",
        }

    def test_commands_nested_commands_options_and_choices_follow_parser(self) -> None:
        self.write_json(
            self.workspace / "profiles" / "review.json",
            self.profile_record("review", "classic"),
        )
        mode, values = self.candidates("")
        self.assertEqual(mode, "candidates")
        self.assertIn("completion", values)
        self.assertIn("worktree", values)
        self.assertIn("scope", values)

        self.assertEqual(
            self.candidates("worktree", ""),
            ("candidates", ["create", "list", "remove"]),
        )
        self.assertEqual(
            self.candidates("scope", ""),
            ("candidates", ["create", "list", "release", "show"]),
        )
        self.assertEqual(
            self.candidates("topology", "show", "default", "--service", "s"),
            ("candidates", ["server"]),
        )
        self.assertEqual(
            self.candidates("init", "--with", "c"),
            ("candidates", ["classic"]),
        )
        _, profile_names = self.candidates("profile", "set", "")
        self.assertEqual(profile_names, ["review"])
        _, profile_values = self.candidates("profile", "set", "review", "")
        self.assertIn("libatrinik", profile_values)
        mode, values = self.candidates("logs", "default", "server", "--f")
        self.assertEqual(mode, "candidates")
        self.assertEqual(values, ["--follow"])

    def test_completed_scope_names_refresh_without_loading_workspace(self) -> None:
        self.write_json(
            self.workspace / "scopes" / "review" / "scope.json",
            {"schema_version": 1, "status": "complete", "name": "review"},
        )
        self.write_json(
            self.workspace / "scopes" / "partial" / "scope.json",
            {"schema_version": 1, "status": "creating", "name": "partial"},
        )
        self.write_json(
            self.workspace / "scopes" / "review" / "release-journal.json",
            {
                "schema_version": 1,
                "scope": "review",
                "status": "complete",
            },
        )
        self.assertEqual(
            self.candidates("scope", "show", ""),
            ("candidates", ["review"]),
        )
        self.assertEqual(
            self.candidates("scope", "release", ""),
            ("candidates", ["review"]),
        )

    def test_consumed_and_mutually_exclusive_options_are_suppressed(self) -> None:
        mode, values = self.candidates("cleanup", "--dry-run", "--", "")
        self.assertEqual(mode, "none")
        self.assertEqual(values, [])

        words = ["atrinik", "cleanup", "--dry-run", "-"]
        mode, values = complete(parser(), self.wrapper, words, 3)
        self.assertEqual(mode, "candidates")
        self.assertNotIn("--apply", values)
        self.assertNotIn("--dry-run", values)
        self.assertIn("--scope", values)

        words = ["atrinik", "cleanup", "--scope", "builds", "-"]
        _, values = complete(parser(), self.wrapper, words, 4)
        self.assertIn("--scope", values)

        _, values = self.candidates("up", "-")
        self.assertTrue(
            {"--state", "--temporary-state", "--default-state"} <= set(values)
        )
        _, values = self.candidates("up", "--temporary-state", "-")
        self.assertTrue(
            {"--state", "--temporary-state", "--default-state"}.isdisjoint(
                values
            )
        )
        _, values = self.candidates(
            "topology", "show", "default", "--default-state", "-"
        )
        self.assertTrue(
            {"--state", "--temporary-state", "--default-state"}.isdisjoint(
                values
            )
        )

    def test_run_remainder_and_double_dash_stop_wrapper_completion(self) -> None:
        for words in (
            ["atrinik", "run", "server", "--", "--ver"],
            ["atrinik", "run", "client", "forwarded"],
        ):
            self.assertEqual(
                complete(parser(), self.wrapper, words, len(words) - 1),
                ("none", []),
            )

    def test_manifest_identities_deduplicate_aliases_and_keep_variants(self) -> None:
        mode, values = self.candidates("status", "")
        self.assertEqual(mode, "candidates")
        self.assertEqual(values.count("classic"), 1)
        self.assertIn("classic-client", values)
        self.assertIn("content", values)
        self.assertNotIn("content-1x", values)

        mode, values = self.candidates("build", "--profile", "classic", "")
        self.assertEqual(mode, "candidates")
        self.assertIn("all", values)
        self.assertIn("libatrinik", values)
        self.assertIn("classic-server", values)

        _, default_values = self.candidates("build", "--profile", "default", "")
        self.assertIn("metaserver-worker", default_values)
        self.assertNotIn("libatrinik", default_values)
        self.assertNotIn("all", default_values)
        _, path_values = self.candidates("path", "--profile", "classic", "")
        self.assertIn("classic-client", path_values)
        self.assertNotIn("website", path_values)

    def test_profiles_refresh_and_malformed_records_fail_softly(self) -> None:
        profiles = self.workspace / "profiles"
        self.write_json(
            profiles / "review.json",
            self.profile_record("review", "classic"),
        )
        self.write_json(profiles / "wrong.json", {"name": "different"})
        (profiles / "broken.json").write_text("{", encoding="utf-8")

        _, before = self.candidates("profile", "show", "")
        self.assertIn("default", before)
        self.assertIn("classic", before)
        self.assertIn("review", before)
        self.assertNotIn("wrong", before)
        self.assertNotIn("broken", before)

        (profiles / "review.json").unlink()
        _, after = self.candidates("profile", "show", "")
        self.assertNotIn("review", after)

    def test_current_sound_profiles_are_available_to_completion(self) -> None:
        source = self.profile_record("source-sound", "classic")
        source.update(
            {
                "schema_version": 5,
                "sound_mode": "source",
                "sound_release": None,
            }
        )
        released = self.profile_record("released-sound", "classic")
        released.update(
            {
                "schema_version": 5,
                "sound_mode": "released",
                "sound_release": {
                    "archive_sha256": "a" * 64,
                    "asset_url": (
                        "https://github.com/atrinik/sound/releases/download/v1.2.3/"
                        "atrinik-sound-classic-runtime-1.2.3.tar.gz"
                    ),
                    "manifest_schema_version": 1,
                    "output_tree_sha256": "b" * 64,
                    "product": "atrinik-sound-classic-runtime",
                    "product_version": "1.2.3",
                    "release_manifest_sha256": "c" * 64,
                    "repository": "atrinik/sound",
                    "schema_sha256": "d" * 64,
                    "source_commit": "e" * 40,
                    "source_manifest_sha256": "f" * 64,
                    "source_tree": "1" * 40,
                    "tag": "v1.2.3",
                    "toolchain_sha256": "2" * 64,
                },
            }
        )
        malformed = dict(released)
        malformed["name"] = "malformed-sound"
        malformed["sound_release"] = None
        invalid_coordinates = json.loads(json.dumps(released))
        invalid_coordinates["name"] = "invalid-coordinates"
        invalid_coordinates["sound_release"]["asset_url"] = (
            "https://example.invalid/mutable.tar.gz"
        )
        cross_stack = self.profile_record("cross-stack-sound", "default")
        cross_stack.update(
            {
                "schema_version": 5,
                "sound_mode": "local-playtest",
                "sound_release": None,
            }
        )
        profiles = self.workspace / "profiles"
        self.write_json(profiles / "source-sound.json", source)
        self.write_json(profiles / "released-sound.json", released)
        self.write_json(profiles / "malformed-sound.json", malformed)
        self.write_json(profiles / "invalid-coordinates.json", invalid_coordinates)
        self.write_json(profiles / "cross-stack-sound.json", cross_stack)

        _, names = self.candidates("profile", "show", "")
        self.assertIn("source-sound", names)
        self.assertIn("released-sound", names)
        self.assertNotIn("malformed-sound", names)
        self.assertNotIn("invalid-coordinates", names)
        self.assertNotIn("cross-stack-sound", names)

    def test_profiles_reject_cross_checkout_and_invalid_migration_selectors(self) -> None:
        record = self.profile_record("broken", "classic")
        record["components"]["classic-client"] = {
            "kind": "worktree",
            "value": "feature",
        }
        self.write_json(self.workspace / "profiles" / "broken.json", record)
        _, names = self.candidates("profile", "show", "")
        self.assertNotIn("broken", names)
        self.assertEqual(
            self.candidates("build", "--profile", "broken", ""),
            ("candidates", []),
        )

        migrated = self.profile_record("migrated", "classic")
        migrated["components"]["content-1x"] = {
            "kind": "migrated-worktree",
            "value": "/tmp/not-managed/content-1x",
        }
        self.write_json(self.workspace / "profiles" / "migrated.json", migrated)
        _, names = self.candidates("profile", "show", "")
        self.assertNotIn("migrated", names)

    def test_worktree_labels_are_filtered_by_selected_physical_checkout(self) -> None:
        (self.workspace / "worktrees" / "content" / "main-maps").mkdir(parents=True)
        (self.workspace / "worktrees" / "content" / "classic-maps").mkdir(
            parents=True
        )
        (self.workspace / "worktrees" / "content" / "main-maps" / ".git").write_text(
            "gitdir: /tmp/main-maps\n", encoding="utf-8"
        )
        (
            self.workspace / "worktrees" / "content" / "classic-maps" / ".git"
        ).write_text("gitdir: /tmp/classic-maps\n", encoding="utf-8")
        _, main_values = self.candidates("worktree", "remove", "content", "")
        _, classic_values = self.candidates(
            "worktree", "remove", "content", ""
        )
        self.assertEqual(main_values, ["classic-maps", "main-maps"])
        self.assertEqual(classic_values, ["classic-maps", "main-maps"])

    def test_profile_role_selects_owning_checkout_for_worktrees(self) -> None:
        stack = parser()  # Ensure a fresh parser does not retain process state.
        del stack
        self.write_json(
            self.workspace / "profiles" / "review.json",
            self.profile_record("review", "classic"),
        )
        (self.workspace / "worktrees" / "classic" / "feature").mkdir(parents=True)
        (self.workspace / "worktrees" / "classic" / "feature" / ".git").write_text(
            "gitdir: /tmp/feature\n", encoding="utf-8"
        )
        words = [
            "atrinik",
            "profile",
            "set",
            "review",
            "server",
            "--worktree",
            "",
        ]
        self.assertEqual(
            complete(parser(), self.wrapper, words, len(words) - 1),
            ("candidates", ["feature"]),
        )

    def test_state_scenario_and_topology_records_refresh_without_secrets(self) -> None:
        self.write_json(
            self.workspace / "states.json",
            {
                "schema_version": 1,
                "states": {
                    "review": "/tmp/review",
                    "scenario-issue-292": str(
                        self.workspace / "scenarios" / "issue-292" / "state"
                    ),
                },
            },
        )
        self.write_json(
            self.workspace / "scenarios" / "issue-292" / "scenario.json",
            self.scenario_record("issue-292"),
        )
        self.write_json(
            self.workspace
            / "scenarios"
            / "issue-292"
            / ".atrinik-workspace-managed.json",
            {"schema_version": 1, "purpose": "test-scenario"},
        )
        (self.workspace / "scenarios" / "issue-292" / "password").write_text(
            "secret", encoding="utf-8"
        )
        self.write_json(
            self.workspace
            / "topologies"
            / "completion-review"
            / ".atrinik-workspace-managed.json",
            {"schema_version": 1, "purpose": "topology:completion-review"},
        )
        self.write_json(
            self.workspace / "topologies" / "completion-review" / "status.json",
            {
                "schema_version": 1,
                "name": "completion-review",
                "profile": "classic",
                "stack": "classic",
                "providers": {"server": "classic-server"},
                "dependencies": ["server"],
                "state": "/tmp/completion-state",
                "build_root": "/tmp/completion-build",
                "resolved": {
                    "classic-server": {
                        "path": "/tmp/classic/server",
                        "checkout_path": "/tmp/classic",
                        "checkout": "classic",
                        "repository": "atrinik/classic",
                        "branch": "main",
                        "source": "server",
                        "head": "b" * 40,
                        "dirty": False,
                    }
                },
                "endpoint": {
                    "host": "127.0.0.1",
                    "port": 13327,
                    "fingerprint": "c" * 64,
                },
                "ready": True,
                "started_at": "2026-08-10T00:00:00Z",
                "stopped_at": None,
                "supervisor": {"pid": 123, "start_time": "1"},
                "services": {
                    "server": {
                        "pid": 124,
                        "start_time": "2",
                        "status": "running",
                        "exit_code": None,
                        "log": "/tmp/server.log",
                        "cwd": "/tmp/classic/server",
                    }
                },
            },
        )

        self.assertEqual(
            self.candidates("topology", "show", "default", "--state", ""),
            ("candidates", ["default", "review", "scenario-issue-292"]),
        )
        with mock.patch(
            "atrinik_workspace.completion._json", wraps=completion._json
        ) as load_metadata:
            self.assertEqual(
                self.candidates("scenario", "show", ""),
                ("candidates", ["issue-292"]),
            )
        self.assertNotIn(
            self.workspace / "scenarios" / "issue-292" / "password",
            [call.args[0] for call in load_metadata.call_args_list],
        )
        states = json.loads(
            (self.workspace / "states.json").read_text(encoding="utf-8")
        )
        del states["states"]["scenario-issue-292"]
        self.write_json(self.workspace / "states.json", states)
        self.assertEqual(
            self.candidates("scenario", "show", ""),
            ("candidates", []),
        )
        self.assertEqual(
            self.candidates("logs", ""),
            ("candidates", ["completion-review"]),
        )
        self.assertEqual(
            self.candidates("state", "promote", ""),
            ("candidates", ["completion-review"]),
        )
        self.assertEqual(
            self.candidates(
                "state", "promote", "completion-review", ""
            ),
            ("candidates", []),
        )

        status_path = self.workspace / "topologies" / "completion-review" / "status.json"
        status = json.loads(status_path.read_text(encoding="utf-8"))
        generation = "d" * 64
        status["control"] = {
            "socket": str(
                control_socket_path(
                    self.workspace / "topologies" / "completion-review",
                    generation,
                )
            ),
            "generation": generation,
            "lease": {"device": 1, "inode": 2},
        }
        status["supervisor"]["generation"] = generation
        status["services"]["server"]["generation"] = generation
        self.write_json(status_path, status)
        self.assertEqual(
            self.candidates("logs", ""),
            ("candidates", ["completion-review"]),
        )
        status["control"]["socket"] = "/tmp/external/control.sock"
        self.write_json(status_path, status)
        self.assertEqual(self.candidates("logs", ""), ("candidates", []))
        status["control"]["socket"] = str(
            control_socket_path(
                self.workspace / "topologies" / "completion-review",
                generation,
            )
        )

        status["profile"] = "deleted-profile"
        self.write_json(status_path, status)
        self.assertEqual(
            self.candidates("logs", ""),
            ("candidates", ["completion-review"]),
        )

        broken_scenario = self.scenario_record("broken")
        broken_scenario["providers"] = {}
        broken_scenario["resolved"] = {}
        self.write_json(
            self.workspace / "scenarios" / "broken" / "scenario.json",
            broken_scenario,
        )
        self.write_json(
            self.workspace / "scenarios" / "broken" / completion.MANAGED_MARKER,
            {"schema_version": 1, "purpose": "test-scenario"},
        )
        _, scenarios = self.candidates("scenario", "show", "")
        self.assertNotIn("broken", scenarios)

        status["dependencies"] = [[]]
        self.write_json(status_path, status)
        self.assertEqual(self.candidates("logs", ""), ("candidates", []))

        status["dependencies"] = ["server"]
        status["services"] = {}
        status["endpoint"] = None
        status["error"] = "RuntimeError: startup failed"
        self.write_json(status_path, status)
        self.assertEqual(
            self.candidates("logs", ""),
            ("candidates", ["completion-review"]),
        )

        status["error"] = ""
        self.write_json(status_path, status)
        self.assertEqual(self.candidates("logs", ""), ("candidates", []))

        del status["error"]
        status["services"] = {
            "server": {
                "pid": 124,
                "start_time": "2",
                "status": "running",
                "exit_code": None,
                "log": "/tmp/server.log",
                "cwd": "/tmp/classic/server",
            }
        }
        status["endpoint"] = {
            "host": "127.0.0.1",
            "port": 13327,
            "fingerprint": None,
        }
        status["ready"] = True
        self.write_json(status_path, status)
        self.assertEqual(self.candidates("logs", ""), ("candidates", []))

    def test_unsafe_and_control_character_records_are_never_candidates(self) -> None:
        root = self.workspace / "worktrees" / "client"
        root.mkdir(parents=True)
        safe = "safe-label"
        unsafe = [
            "with space",
            "quote'",
            'quote"',
            "$(touch-x)",
            "`touch-x`",
            "glob*",
            "brace{x}",
            "line\nbreak",
        ]
        (root / safe).mkdir()
        (root / safe / ".git").write_text("gitdir: /tmp/safe\n", encoding="utf-8")
        for name in unsafe:
            (root / name).mkdir()
            (root / name / ".git").write_text("gitdir: /tmp/unsafe\n", encoding="utf-8")
        _, values = self.candidates("worktree", "remove", "client", "")
        self.assertEqual(values, [safe])

    def test_oversized_dynamic_directories_fail_closed_without_unbounded_scan(self) -> None:
        root = self.workspace / "worktrees" / "client"
        root.mkdir(parents=True)
        for index in range(257):
            (root / f"label-{index:03d}").mkdir()
        self.assertEqual(
            self.candidates("worktree", "remove", "client", ""),
            ("candidates", []),
        )

        scenarios = self.workspace / "scenarios"
        for index in range(completion._MAX_RECORD_ENTRIES + 1):
            (scenarios / f"scenario-{index:03d}").mkdir(parents=True)
        self.assertEqual(
            self.candidates("scenario", "show", ""),
            ("candidates", []),
        )

    def test_path_arguments_delegate_to_native_shell_completion(self) -> None:
        self.assertEqual(
            self.candidates("state", "add", "review", "--path", ""),
            ("path", [""]),
        )
        self.assertEqual(
            self.candidates("supply-chain", "versions", "--output", ""),
            ("path", [""]),
        )
        words = ["atrinik", "state", "add", "review", "--path=some/file"]
        self.assertEqual(
            complete(parser(), self.wrapper, words, len(words) - 1),
            ("path", ["some/file"]),
        )

    def test_missing_workspace_is_quiet_read_only_and_avoids_dispatch(self) -> None:
        missing = self.root / "missing"
        with mock.patch.dict(os.environ, {"ATRINIK_WORKSPACE_DIR": str(missing)}):
            stdout = io.StringIO()
            stderr = io.StringIO()
            with (
                mock.patch("atrinik_workspace.cli.Workspace") as workspace,
                mock.patch("subprocess.run") as run,
                redirect_stdout(stdout),
                redirect_stderr(stderr),
            ):
                result = main(["__complete", "1", "--", "atrinik", ""])
        self.assertEqual(result, 0)
        self.assertTrue(stdout.getvalue().startswith("candidates\n"))
        self.assertEqual(stderr.getvalue(), "")
        self.assertFalse(missing.exists())
        workspace.assert_not_called()
        run.assert_not_called()

    def test_completion_has_no_mutating_network_or_dispatch_path(self) -> None:
        stdout = io.StringIO()
        mutations = [
            "mkdir", "write_text", "write_bytes", "unlink", "rename", "replace",
        ]
        with ExitStack() as stack:
            for name in mutations:
                stack.enter_context(mock.patch.object(Path, name))
            workspace = stack.enter_context(
                mock.patch("atrinik_workspace.cli.Workspace")
            )
            popen = stack.enter_context(mock.patch("subprocess.Popen"))
            run = stack.enter_context(mock.patch("subprocess.run"))
            call = stack.enter_context(mock.patch("subprocess.call"))
            check_call = stack.enter_context(mock.patch("subprocess.check_call"))
            check_output = stack.enter_context(mock.patch("subprocess.check_output"))
            socket_api = stack.enter_context(mock.patch("socket.socket"))
            connect = stack.enter_context(mock.patch("socket.create_connection"))
            stack.enter_context(redirect_stdout(stdout))
            result = main(["__complete", "1", "--", "atrinik", ""])
        self.assertEqual(result, 0)
        self.assertTrue(stdout.getvalue().startswith("candidates\n"))
        for forbidden in (
            workspace, popen, run, call, check_call, check_output, socket_api, connect
        ):
            forbidden.assert_not_called()

    def test_fresh_completion_avoids_heavy_dispatch_imports(self) -> None:
        script = "\n".join(
            [
                "import sys",
                "from atrinik_workspace.cli import main",
                "main(['__complete', '1', '--', 'atrinik', ''])",
                "assert 'atrinik_workspace.workspace' not in sys.modules",
                "assert 'atrinik_workspace.supply_chain' not in sys.modules",
            ]
        )
        result = subprocess.run(
            [sys.executable, "-c", script], capture_output=True, text=True, cwd=ROOT
        )
        self.assertEqual(result.returncode, 0, result.stderr)

    def test_metadata_reads_are_bounded_and_non_regular_files_fail_closed(self) -> None:
        metadata = self.wrapper / "metadata.json"
        metadata.write_text("{}", encoding="utf-8")
        supplied = 0

        def growing_read(descriptor: int, size: int) -> bytes:
            nonlocal supplied
            supplied += size
            return b" " * size

        with mock.patch("atrinik_workspace.completion.os.read", side_effect=growing_read):
            self.assertIsNone(completion._json(metadata))
        self.assertEqual(supplied, completion._MAX_METADATA_BYTES + 1)

        fifo = self.wrapper / "fifo.json"
        os.mkfifo(fifo)
        self.assertIsNone(completion._json(fifo))

    def test_workspace_markers_symlinks_and_unmanaged_worktrees_fail_closed(self) -> None:
        (self.workspace / ".atrinik-workspace.json").write_text(
            "{}", encoding="utf-8"
        )
        self.assertEqual(self.candidates("profile", "show", ""), (
            "candidates", ["classic", "default"]
        ))

        (self.workspace / ".atrinik-workspace.json").write_text(
            json.dumps({"schema_version": 1}), encoding="utf-8"
        )
        external = self.root / "external"
        (external / "client" / "escaped").mkdir(parents=True)
        (external / "client" / "escaped" / ".git").write_text(
            "gitdir: /tmp/escaped\n", encoding="utf-8"
        )
        (self.workspace / "worktrees").symlink_to(external, target_is_directory=True)
        self.assertEqual(
            self.candidates("worktree", "remove", "client", ""),
            ("candidates", []),
        )
        paths = completion._paths(self.wrapper)
        self.assertIsNotNone(paths)
        with mock.patch(
            "atrinik_workspace.completion._workspace_descriptor", return_value=None
        ):
            self.assertEqual(completion._registered_states(paths), {})

    def test_script_generation_avoids_workspace_dispatch(self) -> None:
        with mock.patch("atrinik_workspace.cli.Workspace") as workspace:
            stdout = io.StringIO()
            with redirect_stdout(stdout):
                result = main(["completion", "bash"])
        self.assertEqual(result, 0)
        self.assertIn("_atrinik_completion", stdout.getvalue())
        workspace.assert_not_called()

    def test_malformed_manifest_preserves_static_candidates(self) -> None:
        (self.wrapper / "components.json").write_text("{", encoding="utf-8")
        self.assertEqual(
            self.candidates("topology", "show", "default", "--service", ""),
            ("candidates", ["client", "server"]),
        )
        self.assertEqual(
            self.candidates("status", ""),
            ("candidates", []),
        )

    def test_parser_value_actions_are_classified_for_completion_drift(self) -> None:
        unclassified: list[str] = []
        for action in classified_actions(parser()):
            if isinstance(action, argparse._SubParsersAction):
                continue
            if action.nargs == 0 or action.choices is not None:
                continue
            if not hasattr(action, "completion_kind"):
                unclassified.append(action.dest)
        self.assertEqual(unclassified, [])

    def test_generated_scripts_are_deterministic_and_parse_when_shell_exists(self) -> None:
        for shell in ("bash", "zsh", "fish"):
            first = shell_script(shell)
            self.assertEqual(first, shell_script(shell))
            self.assertNotIn(".bashrc", first)
            self.assertNotIn("config.fish", first)
            executable = shutil.which(shell)
            if executable is None:
                continue
            result = subprocess.run(
                [executable, "-n"], input=first, text=True, capture_output=True
            )
            self.assertEqual(result.returncode, 0, result.stderr)

        shellcheck = shutil.which("shellcheck")
        if shellcheck is not None:
            result = subprocess.run(
                [shellcheck, "--shell=bash", "-"],
                input=shell_script("bash"),
                text=True,
                capture_output=True,
            )
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)

    def test_bash_adapter_supports_an_absolute_invocation_path_with_spaces(self) -> None:
        bash = shutil.which("bash")
        if bash is None:
            self.skipTest("Bash is unavailable")
        directory = self.root / "absolute path with spaces"
        directory.mkdir()
        invocation = directory / "atrinik"
        invocation.symlink_to(ROOT / "atrinik")
        program = shell_script("bash") + "\n" + "\n".join(
            [
                f"COMP_WORDS=({shlex.quote(str(invocation))} profile '')",
                "COMP_CWORD=2",
                "_atrinik_completion",
                "printf '%s\\n' \"${COMPREPLY[@]}\"",
            ]
        )
        environment = {**os.environ, "PYTHONPATH": str(ROOT)}
        result = subprocess.run(
            [bash],
            input=program,
            text=True,
            capture_output=True,
            cwd=ROOT,
            env=environment,
        )
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(
            result.stdout.splitlines(), ["create", "set", "show", "sound-mode"]
        )

    def test_bash_adapter_completes_equals_form_paths(self) -> None:
        bash = shutil.which("bash")
        if bash is None:
            self.skipTest("Bash is unavailable")
        target = self.root / "path-target"
        target.mkdir()
        (target / "finished").touch()
        current = f"--path={target}/fin"
        program = shell_script("bash") + "\n" + "\n".join(
            [
                f"COMP_WORDS=(./atrinik state add review {shlex.quote(current)})",
                "COMP_CWORD=4",
                "_atrinik_completion",
                "printf '%s\\n' \"${COMPREPLY[@]}\"",
            ]
        )
        result = subprocess.run(
            [bash], input=program, text=True, capture_output=True, cwd=ROOT
        )
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(result.stdout.splitlines(), [f"--path={target}/finished"])

    def test_zsh_adapter_smoke(self) -> None:
        zsh = shutil.which("zsh")
        if zsh is None:
            self.skipTest("Zsh is unavailable")
        program = "\n".join(
            [
                "function compdef { : }",
                "function compadd { print -l -- \"$@\" }",
                "source <(./atrinik completion zsh)",
                "words=(./atrinik profile '')",
                "CURRENT=3",
                "_atrinik",
            ]
        )
        result = subprocess.run(
            [zsh, "-c", program], text=True, capture_output=True, cwd=ROOT
        )
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(
            result.stdout.splitlines(),
            ["--", "create", "set", "show", "sound-mode"],
        )

    def test_zsh_adapter_loads_from_fpath_with_real_compinit(self) -> None:
        zsh = shutil.which("zsh")
        if zsh is None:
            self.skipTest("Zsh is unavailable")
        functions = self.root / "zfunc"
        functions.mkdir()
        (functions / "_atrinik").write_text(shell_script("zsh"), encoding="utf-8")
        cache = self.root / "zcompdump"
        program = "\n".join(
            [
                f"fpath=({shlex.quote(str(functions))} $fpath)",
                "autoload -Uz compinit",
                f"compinit -d {shlex.quote(str(cache))}",
                "function compadd { print -l -- \"$@\" }",
                "words=(./atrinik profile '')",
                "CURRENT=3",
                "_atrinik",
            ]
        )
        result = subprocess.run(
            [zsh, "-c", program], text=True, capture_output=True, cwd=ROOT
        )
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(
            result.stdout.splitlines(),
            ["--", "create", "set", "show", "sound-mode"],
        )

    def test_fish_adapter_smoke(self) -> None:
        fish = shutil.which("fish")
        if fish is None:
            self.skipTest("Fish is unavailable")
        program = " ".join(
            [
                "function commandline;",
                "switch $argv[1];",
                "case -opc; printf '%s\\n' ./atrinik profile;",
                "case -ct; printf '\\n';",
                "end; end;",
                "./atrinik completion fish | source;",
                "__atrinik_completion",
            ]
        )
        environment = {
            **os.environ,
            "XDG_CONFIG_HOME": str(self.root / "fish-config"),
            "XDG_DATA_HOME": str(self.root / "fish-data"),
        }
        result = subprocess.run(
            [fish, "--no-config", "-c", program],
            text=True,
            capture_output=True,
            cwd=ROOT,
            env=environment,
        )
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(
            result.stdout.splitlines(), ["create", "set", "show", "sound-mode"]
        )


if __name__ == "__main__":
    unittest.main()
