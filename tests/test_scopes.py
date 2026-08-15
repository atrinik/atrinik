from __future__ import annotations

import copy
import json
import os
from concurrent.futures import ThreadPoolExecutor
from pathlib import Path
import shlex
import shutil
import subprocess
import tempfile
import threading
import unittest
from unittest import mock

from atrinik_workspace.model import AtomicJsonCommitUncertain, MANAGED_MARKER, WorkspaceError
from atrinik_workspace.cli import main
from atrinik_workspace.cleanup import Cleanup
import atrinik_workspace.scopes as scopes_module
from atrinik_workspace.scopes import SCOPE_FAILURE_BOUNDARIES_ENV
from atrinik_workspace.scopes import ScopeLifecycle
from atrinik_workspace.workspace import BUILD_METADATA, Workspace


ROOT = Path(__file__).resolve().parents[1]


def command(*arguments: str, cwd: Path) -> str:
    completed = subprocess.run(
        list(arguments), cwd=cwd, check=True, capture_output=True, text=True
    )
    return completed.stdout.strip()


class ScopeLifecycleTests(unittest.TestCase):
    def setUp(self) -> None:
        self.temporary = tempfile.TemporaryDirectory()
        self.root = Path(self.temporary.name)
        self.wrapper = self.root / "wrapper"
        self.wrapper.mkdir()
        shutil.copy2(ROOT / "components.json", self.wrapper / "components.json")
        self.workspace_directory = self.root / "workspace"
        self.environment = mock.patch.dict(
            os.environ, {"ATRINIK_WORKSPACE_DIR": str(self.workspace_directory)}
        )
        self.environment.start()
        self.remote_matcher = mock.patch(
            "atrinik_workspace.workspace._remote_matches", return_value=True
        )
        self.remote_matcher.start()
        self.workspace = Workspace(self.wrapper)
        self.workspace.paths.ensure()

    def tearDown(self) -> None:
        self.workspace.close()
        self.remote_matcher.stop()
        self.environment.stop()
        self.temporary.cleanup()

    def make_checkout(self, checkout_name: str) -> Path:
        checkout = self.workspace.manifest.by_checkout[checkout_name]
        origin = self.root / "origins" / f"{checkout_name}.git"
        origin.parent.mkdir(exist_ok=True)
        command("git", "init", "--bare", str(origin), cwd=self.root)
        seed = self.root / "seeds" / checkout_name
        seed.mkdir(parents=True)
        command("git", "init", "-b", checkout.branch, cwd=seed)
        command("git", "config", "user.name", "Tests", cwd=seed)
        command("git", "config", "user.email", "tests@example.invalid", cwd=seed)
        (seed / "README").write_text(f"{checkout_name}\n", encoding="utf-8")
        for component in self.workspace.manifest.components:
            if component.checkout_name != checkout_name or component.source == ".":
                continue
            (seed / component.source).mkdir(parents=True, exist_ok=True)
            (seed / component.source / ".keep").write_text("\n", encoding="utf-8")
        command("git", "add", ".", cwd=seed)
        command("git", "commit", "-m", "feat: seed", cwd=seed)
        command("git", "remote", "add", "origin", str(origin), cwd=seed)
        command("git", "push", "-u", "origin", checkout.branch, cwd=seed)
        command("git", "symbolic-ref", "HEAD", f"refs/heads/{checkout.branch}", cwd=origin)
        destination = self.wrapper / checkout.path
        command("git", "clone", str(origin), str(destination), cwd=self.root)
        return destination

    def test_completed_retry_is_exact_and_conflicts_do_not_overwrite(self) -> None:
        self.make_checkout("client")
        first = self.workspace.scope_create(["client"], name="retry")
        retried = self.workspace.scope_create(["client"], name="retry")
        self.assertEqual(retried, first)
        with self.assertRaisesRegex(WorkspaceError, "different coordinates"):
            self.workspace.scope_create(
                ["client"], name="retry", branches=["client=feat/conflict"]
            )
        self.assertEqual(self.workspace.scope_show("retry"), first)

        custom = self.workspace.scope_create(
            ["client"],
            name="custom-retry",
            labels=["client=custom-label"],
            branches=["client=feat/custom-retry"],
            start_points=["client=refs/heads/main"],
        )
        self.assertEqual(
            self.workspace.scope_create(
                ["client"],
                name="custom-retry",
                labels=["client=custom-label"],
                branches=["client=feat/custom-retry"],
                start_points=["client=refs/heads/main"],
            ),
            custom,
        )
        with self.assertRaisesRegex(WorkspaceError, "different coordinates"):
            self.workspace.scope_create(["client"], name="custom-retry")

    def test_invalid_requests_fail_before_publication(self) -> None:
        with self.assertRaisesRegex(WorkspaceError, "at least one component"):
            self.workspace.scope_create([], name="empty")
        with self.assertRaisesRegex(WorkspaceError, "CHECKOUT=VALUE"):
            self.workspace.scope_create(["client"], name="bad-map", labels=["client"])
        with self.assertRaisesRegex(WorkspaceError, "repeats checkout"):
            self.workspace.scope_create(
                ["client"], name="duplicate-map", labels=["client=a", "client=b"]
            )
        with self.assertRaisesRegex(WorkspaceError, "not initialized"):
            self.workspace.scope_create(["client"], name="uninitialized")

        checkout = self.make_checkout("client")
        lifecycle = ScopeLifecycle(self.workspace)

        def preflight(name: str, **overrides: object) -> dict[str, object]:
            arguments = {
                "name": name,
                "components": ["client"],
                "base_profile": "default",
                "labels": {},
                "branches": {},
                "start_points": {},
                "topology": None,
                "state_mode": "temporary",
                "state_name": None,
            }
            arguments.update(overrides)
            return lifecycle._preflight_request(**arguments)

        failures = [
            ("unknown-override", {"labels": {"sound": "review"}}, "unselected"),
            ("bad-mode", {"state_mode": "other"}, "invalid scope state mode"),
            ("temporary-name", {"state_name": "named"}, "does not accept"),
            (
                "default-name",
                {"state_mode": "default", "state_name": "named"},
                "does not accept",
            ),
            ("named-missing", {"state_mode": "named"}, "requires --state"),
            (
                "named-default",
                {"state_mode": "named", "state_name": "default"},
                "--default-state",
            ),
            (
                "named-absent",
                {"state_mode": "named", "state_name": "absent"},
                "does not exist",
            ),
            ("bad-start", {"start_points": {"client": "-option"}}, "must not begin"),
        ]
        for name, overrides, message in failures:
            with self.subTest(name=name), self.assertRaisesRegex(WorkspaceError, message):
                preflight(name, **overrides)

        profile = self.workspace_directory / "profiles" / "scope-profile-exists.json"
        profile.write_text("{}", encoding="utf-8")
        with self.assertRaisesRegex(WorkspaceError, "profile already exists"):
            preflight("profile-exists")
        topology = self.workspace_directory / "topologies" / "occupied"
        topology.mkdir(parents=True)
        with self.assertRaisesRegex(WorkspaceError, "topology namespace"):
            preflight("topology-exists", topology="occupied")
        destination = self.workspace_directory / "worktrees" / "client" / "occupied"
        destination.mkdir(parents=True)
        with self.assertRaisesRegex(WorkspaceError, "worktree path"):
            preflight("path-exists", labels={"client": "occupied"})
        command("git", "branch", "scope/branch-exists/client", cwd=checkout)
        with self.assertRaisesRegex(WorkspaceError, "branch already exists"):
            preflight("branch-exists")
        real_run = subprocess.run

        def fail_show_ref(arguments: list[str], **keywords: object) -> object:
            if "show-ref" in arguments:
                return mock.Mock(returncode=2)
            return real_run(arguments, **keywords)

        with mock.patch("subprocess.run", side_effect=fail_show_ref):
            with self.assertRaisesRegex(WorkspaceError, "cannot preflight scope branch"):
                preflight("branch-error")

    def test_scope_record_schema_rejects_each_identity_family(self) -> None:
        self.make_checkout("client")
        record = self.workspace.scope_create(["client"], name="schema")
        lifecycle = ScopeLifecycle(self.workspace)

        def rejected(candidate: object, message: str) -> None:
            with self.assertRaisesRegex(WorkspaceError, message):
                lifecycle._validate_record(candidate, "schema")

        rejected([], "record is invalid")
        mutations: list[tuple[tuple[object, ...], object, str]] = [
            (("extra",), True, "schema"),
            (("status",), "creating", "identity"),
            (("generation",), "bad", "generation"),
            (("request_sha256",), "bad", "request identity"),
            (("stack",), "missing", "stack"),
            (("created_at",), "", "creation time"),
            (("base_profile",), 1, "base profile"),
            (("requested_components",), [], "requested components"),
            (("worktrees",), [], "worktrees"),
            (("worktrees", 0, "extra"), True, "worktree record"),
            (("worktrees", 0, "checkout"), "missing", "checkout"),
            (("worktrees", 0, "repository"), "other/repository", "repository"),
            (("worktrees", 0, "label"), "", "coordinates"),
            (("worktrees", 0, "path_device"), -1, "path identity"),
            (("worktrees", 0, "logical_components"), [], "coverage"),
            (("worktrees", 0, "path"), "/tmp/replaced", "worktree path"),
            (("worktrees", 0, "primary_path"), "/tmp/replaced", "primary checkout"),
            (("profile", "extra"), True, "profile record"),
            (("profile", "path"), "/tmp/replaced", "profile path"),
            (("profile", "name"), "other", "profile path"),
            (("profile", "immutable"), False, "profile identity"),
            (("profile", "path_inode"), -1, "profile path identity"),
            (("topology", "path"), "/tmp/replaced", "topology"),
            (("state_policy", "mode"), "other", "state policy"),
            (("state_policy", "name"), "unexpected", "state identity"),
            (("commands", "extra"), True, "commands are invalid"),
            (("commands", "up"), "", "command coordinates"),
            (("commands", "paths"), [], "command maps"),
            (("cleanup", "policy"), "other", "cleanup coordinates"),
        ]
        for path, value, message in mutations:
            candidate = copy.deepcopy(record)
            target = candidate
            for key in path[:-1]:
                target = target[key]  # type: ignore[index]
            target[path[-1]] = value  # type: ignore[index]
            with self.subTest(path=path):
                rejected(candidate, message)

        digest_mismatch = copy.deepcopy(record)
        digest_mismatch["base_profile"] = "classic"
        rejected(digest_mismatch, "request digest")

    def test_distinct_scopes_use_different_worktrees_of_one_checkout(self) -> None:
        self.make_checkout("client")
        first = self.workspace.scope_create(["client"], name="first")
        second = self.workspace.scope_create(["client"], name="second")
        self.assertNotEqual(first["worktrees"][0]["path"], second["worktrees"][0]["path"])
        self.assertNotEqual(first["worktrees"][0]["branch"], second["worktrees"][0]["branch"])
        self.assertEqual(first["worktrees"][0]["common_git_dir"], second["worktrees"][0]["common_git_dir"])

    def test_concurrent_distinct_scopes_progress_on_one_physical_checkout(self) -> None:
        self.make_checkout("client")
        checkout = self.workspace.manifest.by_checkout["client"]
        git_request = self.workspace._lease_request(
            "git-admin",
            self.workspace._git_admin_coordinate(
                checkout, self.workspace._primary_path(checkout)
            ),
            "exclusive",
            "hold shared checkout publication",
        )
        preflight_barrier = threading.Barrier(2)
        preflights_complete = threading.Event()
        first_preflights: set[int] = set()
        first_preflights_lock = threading.Lock()
        original_preflight = ScopeLifecycle._preflight_request

        def synchronized_preflight(
            lifecycle: ScopeLifecycle, *arguments: object, **keywords: object
        ) -> dict[str, object]:
            result = original_preflight(lifecycle, *arguments, **keywords)
            thread = threading.get_ident()
            with first_preflights_lock:
                first = thread not in first_preflights
                first_preflights.add(thread)
                if len(first_preflights) == 2:
                    preflights_complete.set()
            if first:
                preflight_barrier.wait(timeout=5)
            return result

        def create(name: str) -> dict[str, object]:
            workspace = Workspace(self.wrapper)
            try:
                return workspace.scope_create(["client"], name=name)
            finally:
                workspace.close()

        with ThreadPoolExecutor(max_workers=2) as executor:
            with self.workspace._resource_locks([git_request]), mock.patch.object(
                ScopeLifecycle,
                "_preflight_request",
                synchronized_preflight,
            ):
                first = executor.submit(create, "parallel-a")
                second = executor.submit(create, "parallel-b")
                self.assertTrue(preflights_complete.wait(timeout=5))
            records = [first.result(timeout=10), second.result(timeout=10)]
        self.assertEqual({record["name"] for record in records}, {"parallel-a", "parallel-b"})
        self.assertEqual(len({record["worktrees"][0]["path"] for record in records}), 2)

    def test_distinct_scope_profiles_build_concurrently_from_returned_coordinates(self) -> None:
        self.make_checkout("sound")
        first = self.workspace.scope_create(["sound"], name="build-a")
        second = self.workspace.scope_create(["sound"], name="build-b")
        barrier = threading.Barrier(2)
        observed: dict[str, str] = {}

        def build_resolved(
            workspace: Workspace,
            target: str,
            profile: str,
            tests: bool,
            targets: list[str],
            selected: dict[str, Path],
            **_options: object,
        ) -> Path:
            self.assertEqual(target, "sound")
            self.assertTrue(tests)
            self.assertEqual(targets, ["sound"])
            observed[profile] = str(selected["sound"])
            barrier.wait(timeout=5)
            return workspace.paths.builds / profile

        def build(record: dict[str, object]) -> Path:
            workspace = Workspace(self.wrapper)
            try:
                return workspace.build("sound", record["profile"]["name"], True)
            finally:
                workspace.close()

        with mock.patch.object(Workspace, "_build_resolved", build_resolved):
            with ThreadPoolExecutor(max_workers=2) as executor:
                results = [
                    future.result(timeout=10)
                    for future in (executor.submit(build, first), executor.submit(build, second))
                ]
        self.assertEqual(len(set(results)), 2)
        self.assertEqual(
            observed[first["profile"]["name"]],
            first["worktrees"][0]["path"],
        )
        self.assertEqual(
            observed[second["profile"]["name"]],
            second["worktrees"][0]["path"],
        )

    def test_same_scope_race_has_one_winner_and_no_unowned_partial(self) -> None:
        self.make_checkout("client")

        def create() -> tuple[str, object]:
            workspace = Workspace(self.wrapper)
            try:
                return "winner", workspace.scope_create(["client"], name="race")
            except WorkspaceError as error:
                return "loser", str(error)
            finally:
                workspace.close()

        with ThreadPoolExecutor(max_workers=2) as executor:
            outcomes = [future.result(timeout=10) for future in (executor.submit(create), executor.submit(create))]
        self.assertEqual([kind for kind, _value in outcomes].count("winner"), 1)
        self.assertEqual([kind for kind, _value in outcomes].count("loser"), 1)
        record = self.workspace.scope_show("race")
        self.assertTrue(Path(record["profile"]["path"]).is_file())
        self.assertTrue(Path(record["worktrees"][0]["path"]).is_dir())

    def test_same_explicit_label_and_branch_race_has_one_complete_winner(self) -> None:
        self.make_checkout("client")

        def create(name: str) -> tuple[str, object]:
            workspace = Workspace(self.wrapper)
            try:
                return "winner", workspace.scope_create(
                    ["client"],
                    name=name,
                    labels=["client=shared-coordinate"],
                    branches=["client=feat/shared-coordinate"],
                )
            except WorkspaceError as error:
                return "loser", str(error)
            finally:
                workspace.close()

        with ThreadPoolExecutor(max_workers=2) as executor:
            outcomes = [
                future.result(timeout=10)
                for future in (
                    executor.submit(create, "coordinate-a"),
                    executor.submit(create, "coordinate-b"),
                )
            ]
        self.assertEqual([kind for kind, _value in outcomes].count("winner"), 1)
        self.assertEqual([kind for kind, _value in outcomes].count("loser"), 1)
        complete = [
            path
            for path in (self.workspace_directory / "scopes").glob("*/scope.json")
        ]
        self.assertEqual(len(complete), 1)

    def test_classic_selectors_publish_one_complete_physical_worktree(self) -> None:
        self.make_checkout("classic")
        record = self.workspace.scope_create(
            ["classic-client", "classic-server"], name="classic-agent", base_profile="classic"
        )
        self.assertEqual(len(record["worktrees"]), 1)
        self.assertEqual(record["worktrees"][0]["checkout"], "classic")
        self.assertEqual(
            set(record["worktrees"][0]["logical_components"]),
            {
                "classic-client", "classic-server", "classic-protocol",
                "classic-libatrinik", "classic-editor",
            },
        )

    def test_json_handoff_contains_supported_exact_commands_without_secrets(self) -> None:
        checkout = self.make_checkout("client")
        record = self.workspace.scope_create(["client"], name="handoff")
        serialized = json.dumps(record, sort_keys=True)
        self.assertNotRegex(serialized.lower(), r"password|credential|secret|token")
        self.assertEqual(
            self.workspace.component_path("client", record["profile"]["name"]),
            Path(record["worktrees"][0]["path"]),
        )
        self.assertNotIn("client", record["commands"]["builds"])
        self.assertTrue(record["commands"]["builds"])
        self.assertTrue(
            all("--profile scope-handoff --test" in value for value in record["commands"]["builds"].values())
        )
        self.assertIn("--temporary-state", record["commands"]["up"])
        self.assertEqual(record["state_policy"]["mode"], "temporary")
        self.assertNotEqual(checkout, Path(record["worktrees"][0]["path"]))

        build_command = next(iter(record["commands"]["builds"].values()))
        build_arguments = shlex.split(build_command)[1:]
        up_arguments = shlex.split(record["commands"]["up"])[1:]
        down_arguments = shlex.split(record["commands"]["down"])[1:]
        with mock.patch("atrinik_workspace.cli.Workspace") as workspace_type:
            cli_workspace = workspace_type.return_value
            cli_workspace.build.return_value = self.root / "build-result"
            cli_workspace.topology_up.return_value = {"ready": True}
            cli_workspace.topology_down.return_value = {"stopped_at": "now"}
            with mock.patch("builtins.print"):
                self.assertEqual(main(build_arguments), 0)
                self.assertEqual(main(up_arguments), 0)
                self.assertEqual(main(down_arguments), 0)
        build_target = build_arguments[1]
        cli_workspace.build.assert_called_once_with(
            build_target,
            record["profile"]["name"],
            True,
            force_reconfigure=False,
            use_ccache=True,
        )
        cli_workspace.topology_up.assert_called_once_with(
            record["topology"]["name"],
            record["profile"]["name"],
            None,
            None,
            None,
            state_mode="temporary",
        )
        cli_workspace.topology_down.assert_called_once_with(
            record["topology"]["name"]
        )

    def test_completed_record_rejects_edited_handoff_commands(self) -> None:
        self.make_checkout("client")
        record = self.workspace.scope_create(["client"], name="edited-command")
        record["commands"]["up"] = "./unexpected-command"
        record_path = (
            self.workspace_directory / "scopes" / "edited-command" / "scope.json"
        )
        record_path.write_text(json.dumps(record), encoding="utf-8")
        with self.assertRaisesRegex(WorkspaceError, "exact coordinates"):
            self.workspace.scope_show("edited-command")

    def test_failure_after_each_publication_boundary_is_journaled(self) -> None:
        self.make_checkout("client")
        for index, boundary in enumerate(
            ("reservation", "worktree:client", "profile-reference", "profile", "scope")
        ):
            name = f"failure-{index}"
            with self.subTest(boundary=boundary), mock.patch.dict(
                os.environ, {SCOPE_FAILURE_BOUNDARIES_ENV: boundary}
            ):
                with self.assertRaisesRegex(WorkspaceError, "injected scope failure"):
                    self.workspace.scope_create(["client"], name=name)
            journal = json.loads(
                (self.workspace_directory / "scopes" / name / "creation-journal.json").read_text(encoding="utf-8")
            )
            self.assertIn(journal["status"], {"rolled-back", "complete"})
            record = self.workspace_directory / "scopes" / name / "scope.json"
            self.assertEqual(record.is_file(), boundary == "scope")
            worktree = self.workspace_directory / "worktrees" / "client" / f"scope-{name}"
            self.assertEqual(worktree.is_dir(), boundary == "scope")
            if boundary == "profile-reference":
                self.assertIn("profile-reference", journal["rollback"])

    def test_uncertain_profile_publication_preserves_every_input(self) -> None:
        self.make_checkout("client")
        original = scopes_module.durable_atomic_json
        profile = self.workspace_directory / "profiles" / "scope-profile-uncertain.json"
        injected = False

        def uncertain(path: Path, value: object) -> None:
            nonlocal injected
            original(path, value)
            if path == profile and not injected:
                injected = True
                raise AtomicJsonCommitUncertain("injected profile fsync uncertainty")

        with mock.patch.object(scopes_module, "durable_atomic_json", side_effect=uncertain):
            with self.assertRaisesRegex(WorkspaceError, "publication is uncertain"):
                self.workspace.scope_create(["client"], name="profile-uncertain")
        worktree = self.workspace_directory / "worktrees" / "client" / "scope-profile-uncertain"
        journal = json.loads(
            (
                self.workspace_directory
                / "scopes"
                / "profile-uncertain"
                / "creation-journal.json"
            ).read_text(encoding="utf-8")
        )
        self.assertEqual(journal["status"], "recovery-required")
        self.assertTrue(profile.is_file())
        self.assertTrue(worktree.is_dir())
        with self.assertRaisesRegex(WorkspaceError, "immutable"):
            self.workspace.set_profile("scope-profile-uncertain", "client", "primary")

    def test_uncertain_completed_record_preserves_and_remains_readable(self) -> None:
        self.make_checkout("client")
        original = scopes_module.durable_atomic_json
        record_path = self.workspace_directory / "scopes" / "record-uncertain" / "scope.json"
        injected = False

        def uncertain(path: Path, value: object) -> None:
            nonlocal injected
            original(path, value)
            if path == record_path and not injected:
                injected = True
                raise AtomicJsonCommitUncertain("injected record fsync uncertainty")

        with mock.patch.object(scopes_module, "durable_atomic_json", side_effect=uncertain):
            with self.assertRaisesRegex(WorkspaceError, "publication is uncertain"):
                self.workspace.scope_create(["client"], name="record-uncertain")
        record = self.workspace.scope_show("record-uncertain")
        self.assertEqual(record["status"], "complete")
        self.assertTrue(Path(record["worktrees"][0]["path"]).is_dir())
        journal = json.loads(
            (
                self.workspace_directory
                / "scopes"
                / "record-uncertain"
                / "creation-journal.json"
            ).read_text(encoding="utf-8")
        )
        self.assertEqual(journal["status"], "recovery-required")

    def test_recoverable_creation_journal_protects_changed_worktree_from_cleanup(self) -> None:
        self.make_checkout("client")
        path = self.workspace_directory / "worktrees" / "client" / "scope-recoverable"

        def fail_after_dirtying(boundary: str) -> None:
            if boundary != "worktree:client":
                return
            (path / "recovery-input").write_text("preserve\n", encoding="utf-8")
            raise WorkspaceError("injected changed recovery input")

        with mock.patch.object(
            ScopeLifecycle, "_maybe_fail", side_effect=fail_after_dirtying
        ):
            with self.assertRaisesRegex(WorkspaceError, "recovery inputs were preserved"):
                self.workspace.scope_create(["client"], name="recoverable")
        journal = json.loads(
            (
                self.workspace_directory
                / "scopes"
                / "recoverable"
                / "creation-journal.json"
            ).read_text(encoding="utf-8")
        )
        self.assertEqual(journal["status"], "recovery-required")
        self.assertIn(
            "scope:recoverable", self.workspace._source_references(path)
        )
        references, errors = Cleanup(self.workspace)._references()
        self.assertFalse(errors)
        self.assertEqual(
            references["scopes"][path.resolve()], ["recoverable"]
        )

    def test_release_is_preview_bound_and_protects_dirty_worktree(self) -> None:
        self.make_checkout("client")
        record = self.workspace.scope_create(["client"], name="release")
        preview = self.workspace.scope_release("release", apply=False)
        self.assertTrue(preview["can_apply"])
        with self.assertRaisesRegex(WorkspaceError, "requires the exact --plan"):
            self.workspace.scope_release("release", apply=True)
        worktree = Path(record["worktrees"][0]["path"])
        (worktree / "dirty").write_text("changed\n", encoding="utf-8")
        changed = self.workspace.scope_release("release", apply=False)
        self.assertFalse(changed["can_apply"])
        self.assertIn(
            "dirty_worktree",
            next(item for item in changed["items"] if item["kind"] == "worktree")["reasons"],
        )
        with self.assertRaisesRegex(WorkspaceError, "changed since preview"):
            self.workspace.scope_release(
                "release", apply=True, plan_sha256=preview["plan_sha256"]
            )

    def test_clean_release_removes_profile_and_worktree_but_retains_record(self) -> None:
        self.make_checkout("client")
        record = self.workspace.scope_create(["client"], name="clean-release")
        preview = self.workspace.scope_release("clean-release", apply=False)
        result = self.workspace.scope_release(
            "clean-release", apply=True, plan_sha256=preview["plan_sha256"]
        )
        self.assertTrue(result["released"])
        self.assertFalse(Path(record["profile"]["path"]).exists())
        self.assertFalse(Path(record["worktrees"][0]["path"]).exists())
        self.assertEqual(self.workspace.scope_show("clean-release"), record)
        with self.assertRaisesRegex(WorkspaceError, "release has started"):
            self.workspace.scope_create(["client"], name="clean-release")

    def test_resumed_release_requires_and_preserves_exact_plan(self) -> None:
        self.make_checkout("client")
        record = self.workspace.scope_create(["client"], name="resume-release")
        preview = self.workspace.scope_release("resume-release", apply=False)
        release_path = (
            self.workspace_directory
            / "scopes"
            / "resume-release"
            / "release-journal.json"
        )
        profile_path = Path(record["profile"]["path"])
        profile_path.unlink()
        self.workspace._remove_physical_reference(profile_path)
        scopes_module.durable_atomic_json(
            release_path,
            {
                "schema_version": 1,
                "scope": "resume-release",
                "generation": record["generation"],
                "plan_sha256": preview["plan_sha256"],
                "plan": {
                    key: preview[key]
                    for key in ("schema_version", "scope", "generation", "items")
                },
                "status": "applying",
                "completed": ["profile"],
                "in_flight": None,
                "updated_at": "2026-08-14T00:00:00Z",
            },
        )
        self.workspace.scope_release(
            "resume-release", apply=True, plan_sha256=preview["plan_sha256"]
        )
        journal = json.loads(release_path.read_text(encoding="utf-8"))
        self.assertIn("profile", journal["completed"])
        self.assertIn("worktree:client", journal["completed"])

    def test_release_recovers_each_destructive_before_journal_crash(self) -> None:
        self.make_checkout("client")

        build_record = self.workspace.scope_create(["client"], name="crash-build")
        key = "b" * 64
        build_root = (
            self.workspace.paths.builds
            / "profiles"
            / f"{build_record['profile']['name']}-{key}"
        )
        build_root.mkdir(parents=True)
        (build_root / BUILD_METADATA).write_text(
            json.dumps({"profile": build_record["profile"]["name"], "key": key}),
            encoding="utf-8",
        )
        (build_root / MANAGED_MARKER).write_text(
            json.dumps(
                {
                    "schema_version": 1,
                    "purpose": f"profile:{build_record['profile']['name']}:{key}",
                }
            ),
            encoding="utf-8",
        )
        build_plan = self.workspace.scope_release("crash-build", apply=False)
        from atrinik_workspace import workspace as workspace_module

        real_remove = workspace_module.remove_owned_tree
        failed = False

        def remove_then_crash(*args: object, **kwargs: object) -> None:
            nonlocal failed
            real_remove(*args, **kwargs)
            if not failed:
                failed = True
                raise WorkspaceError("crash after build removal")

        with mock.patch.object(
            workspace_module, "remove_owned_tree", side_effect=remove_then_crash
        ):
            with self.assertRaisesRegex(WorkspaceError, "after build removal"):
                self.workspace.scope_release(
                    "crash-build", apply=True, plan_sha256=build_plan["plan_sha256"]
                )
        self.workspace.scope_release(
            "crash-build", apply=True, plan_sha256=build_plan["plan_sha256"]
        )

        profile_record = self.workspace.scope_create(["client"], name="crash-profile")
        profile_plan = self.workspace.scope_release("crash-profile", apply=False)
        real_reference_remove = self.workspace._remove_physical_reference
        failed = False

        def reference_then_crash(path: Path) -> None:
            nonlocal failed
            real_reference_remove(path)
            if not failed:
                failed = True
                raise WorkspaceError("crash after profile unlink")

        with mock.patch.object(
            self.workspace,
            "_remove_physical_reference",
            side_effect=reference_then_crash,
        ):
            with self.assertRaisesRegex(WorkspaceError, "after profile unlink"):
                self.workspace.scope_release(
                    "crash-profile",
                    apply=True,
                    plan_sha256=profile_plan["plan_sha256"],
                )
        self.workspace.scope_release(
            "crash-profile", apply=True, plan_sha256=profile_plan["plan_sha256"]
        )

        worktree_record = self.workspace.scope_create(["client"], name="crash-worktree")
        worktree_plan = self.workspace.scope_release("crash-worktree", apply=False)
        real_git = workspace_module.git
        failed = False

        def git_then_crash(path: Path, *arguments: str, **kwargs: object) -> str:
            nonlocal failed
            result = real_git(path, *arguments, **kwargs)
            if arguments[:2] == ("worktree", "remove") and not failed:
                failed = True
                raise WorkspaceError("crash after worktree removal")
            return result

        with mock.patch.object(workspace_module, "git", side_effect=git_then_crash):
            with self.assertRaisesRegex(WorkspaceError, "after worktree removal"):
                self.workspace.scope_release(
                    "crash-worktree",
                    apply=True,
                    plan_sha256=worktree_plan["plan_sha256"],
                )
        result = self.workspace.scope_release(
            "crash-worktree",
            apply=True,
            plan_sha256=worktree_plan["plan_sha256"],
        )
        self.assertTrue(result["released"])
        self.assertFalse(Path(worktree_record["worktrees"][0]["path"]).exists())

    def test_release_removes_only_exact_scope_build_ownership(self) -> None:
        self.make_checkout("client")
        record = self.workspace.scope_create(["client"], name="build-release")
        key = "a" * 64
        root = (
            self.workspace.paths.builds
            / "profiles"
            / f"{record['profile']['name']}-{key}"
        )
        root.mkdir(parents=True)
        (root / BUILD_METADATA).write_text(
            json.dumps({"profile": record["profile"]["name"], "key": key}),
            encoding="utf-8",
        )
        (root / MANAGED_MARKER).write_text(
            json.dumps(
                {
                    "schema_version": 1,
                    "purpose": f"profile:{record['profile']['name']}:{key}",
                }
            ),
            encoding="utf-8",
        )
        preview = self.workspace.scope_release("build-release", apply=False)
        build = next(item for item in preview["items"] if item["kind"] == "build")
        self.assertEqual(build["disposition"], "eligible")
        self.assertRegex(build["metadata_sha256"], r"^[0-9a-f]{64}$")
        result = self.workspace.scope_release(
            "build-release", apply=True, plan_sha256=preview["plan_sha256"]
        )
        self.assertTrue(result["released"])
        self.assertFalse(root.exists())

    def test_scope_profile_is_immutable(self) -> None:
        self.make_checkout("client")
        record = self.workspace.scope_create(["client"], name="immutable")
        with self.assertRaisesRegex(WorkspaceError, "immutable"):
            self.workspace.set_profile(record["profile"]["name"], "client", "primary")

    def test_release_refuses_identical_profile_path_replacement(self) -> None:
        self.make_checkout("client")
        record = self.workspace.scope_create(["client"], name="replaced-profile")
        profile = Path(record["profile"]["path"])
        content = profile.read_bytes()
        profile.unlink()
        profile.write_bytes(content)
        preview = self.workspace.scope_release("replaced-profile", apply=False)
        item = next(item for item in preview["items"] if item["kind"] == "profile")
        self.assertEqual(item["disposition"], "protected")
        self.assertIn("replaced_profile", item["reasons"])

    def test_release_protects_live_referenced_detached_and_replaced_inputs(self) -> None:
        self.make_checkout("client")

        live = self.workspace.scope_create(["client"], name="live")
        Path(live["topology"]["path"]).mkdir()
        with mock.patch.object(
            self.workspace,
            "topology_status",
            return_value={
                "supervisor": {"running": True, "liveness": "live"},
                "services": {},
                "observation": {},
            },
        ):
            preview = self.workspace.scope_release("live", apply=False)
        self.assertIn(
            "live_topology",
            next(item for item in preview["items"] if item["kind"] == "topology")["reasons"],
        )

        referenced = self.workspace.scope_create(["client"], name="referenced")
        self.workspace.create_profile("outside-reference")
        self.workspace.set_profile(
            "outside-reference",
            "client",
            "worktree",
            referenced["worktrees"][0]["label"],
        )
        preview = self.workspace.scope_release("referenced", apply=False)
        reasons = next(item for item in preview["items"] if item["kind"] == "worktree")["reasons"]
        self.assertTrue(any(reason.startswith("unexpected_references:") for reason in reasons))

        detached = self.workspace.scope_create(["client"], name="detached")
        command("git", "checkout", "--detach", cwd=Path(detached["worktrees"][0]["path"]))
        preview = self.workspace.scope_release("detached", apply=False)
        self.assertIn(
            "detached_or_changed_branch",
            next(item for item in preview["items"] if item["kind"] == "worktree")["reasons"],
        )

        replaced = self.workspace.scope_create(["client"], name="replaced")
        path = Path(replaced["worktrees"][0]["path"])
        moved = path.with_name("moved-replaced")
        path.rename(moved)
        path.mkdir()
        preview = self.workspace.scope_release("replaced", apply=False)
        self.assertIn(
            "replaced_path",
            next(item for item in preview["items"] if item["kind"] == "worktree")["reasons"],
        )

    def test_release_refuses_unreachable_retained_and_active_coordinates(self) -> None:
        self.make_checkout("client")
        unreachable = self.workspace.scope_create(["client"], name="unreachable")
        Path(unreachable["topology"]["path"]).mkdir()
        with mock.patch.object(
            self.workspace,
            "topology_status",
            return_value={
                "supervisor": {"running": True, "liveness": "unreachable"},
                "services": {},
                "observation": {"process_tree_lease": "retained"},
            },
        ):
            preview = self.workspace.scope_release("unreachable", apply=False)
        self.assertIn(
            "unreachable_topology",
            next(item for item in preview["items"] if item["kind"] == "topology")["reasons"],
        )

        retained = self.workspace.scope_create(["client"], name="retained")
        Path(retained["topology"]["path"]).mkdir()
        with mock.patch.object(
            self.workspace,
            "topology_status",
            return_value={
                "supervisor": {"running": False, "liveness": "exited"},
                "services": {},
                "observation": {"runtime_bundle_lease": "retained"},
            },
        ):
            preview = self.workspace.scope_release("retained", apply=False)
        self.assertIn(
            "retained_generation",
            next(item for item in preview["items"] if item["kind"] == "topology")["reasons"],
        )

        busy = self.workspace.scope_create(["client"], name="busy")
        row = busy["worktrees"][0]
        request = self.workspace._lease_request(
            "source",
            self.workspace._source_coordinate(row["checkout"], Path(row["path"])),
            "shared",
            "hold scope source",
        )
        with self.workspace._resource_locks([request]):
            with self.assertRaisesRegex(WorkspaceError, "active resource leases"):
                self.workspace.scope_release("busy", apply=False)

    def test_scope_topology_namespace_requires_recorded_profile_and_state(self) -> None:
        self.make_checkout("client")
        record = self.workspace.scope_create(["client"], name="reserved-topology")
        with self.assertRaisesRegex(WorkspaceError, "reserved by scope"):
            self.workspace.topology_up(
                record["topology"]["name"],
                "default",
                None,
                ["server"],
                state_mode="temporary",
            )

    def test_persistent_scope_state_is_deliberate_and_never_released(self) -> None:
        self.make_checkout("client")
        state = self.root / "persistent-state"
        state.mkdir()
        for name in ("bans", "motd"):
            (state / name).write_text("", encoding="utf-8")
        for name in ("keys", "unique-items"):
            (state / name).mkdir()
        self.workspace.state_add("shared", state)
        named = self.workspace.scope_create(
            ["client"], name="named-state", state_mode="named", state_name="shared"
        )
        preview = self.workspace.scope_release("named-state", apply=False)
        state_item = next(item for item in preview["items"] if item["kind"] == "state")
        self.assertEqual(state_item["disposition"], "retained")
        self.assertEqual(named["state_policy"]["lifecycle"], "never-remove-with-scope")

        default = self.workspace.scope_create(
            ["client"], name="default-state", state_mode="default"
        )
        self.assertEqual(default["state_policy"]["mode"], "default")
        self.assertIn("--default-state", default["commands"]["up"])

    def test_live_scope_a_does_not_block_scope_b_release(self) -> None:
        self.make_checkout("client")
        scope_a = self.workspace.scope_create(["client"], name="scope-a")
        Path(scope_a["topology"]["path"]).mkdir()
        scope_b = self.workspace.scope_create(["client"], name="scope-b")
        live_a = self.workspace._lease_request(
            "topology", scope_a["topology"]["name"], "exclusive", "test live scope A"
        )
        with self.workspace._resource_locks([live_a]):
            preview = self.workspace.scope_release("scope-b", apply=False)
            released = self.workspace.scope_release(
                "scope-b", apply=True, plan_sha256=preview["plan_sha256"]
            )
        self.assertTrue(released["released"])
        self.assertTrue(Path(scope_a["worktrees"][0]["path"]).is_dir())
        self.assertTrue(Path(scope_a["profile"]["path"]).is_file())
        self.assertTrue(Path(scope_a["topology"]["path"]).is_dir())
        self.assertFalse(Path(scope_b["worktrees"][0]["path"]).exists())


if __name__ == "__main__":
    unittest.main()
