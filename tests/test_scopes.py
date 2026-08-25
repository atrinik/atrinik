from __future__ import annotations

import copy
import json
import os
import random
from concurrent.futures import ThreadPoolExecutor
from pathlib import Path
import shlex
import shutil
import subprocess
import tempfile
import threading
import unittest
from unittest import mock

from atrinik_workspace.model import (
    AtomicJsonCommitUncertain,
    MANAGED_MARKER,
    WorkspaceError,
    atomic_json,
)
from atrinik_workspace.cli import main
from atrinik_workspace.cleanup import Cleanup
import atrinik_workspace.locking as locking_module
import atrinik_workspace.scopes as scopes_module
from atrinik_workspace.scopes import SCOPE_FAILURE_BOUNDARIES_ENV
from atrinik_workspace.scopes import ScopeLifecycle
from atrinik_workspace.workspace import (
    BUILD_METADATA,
    TOPOLOGY_STATUS_SCHEMA_VERSION,
    Workspace,
)


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
        if checkout_name == "classic":
            server = seed / "server"
            (server / "tools").mkdir()
            (server / "tools" / ".keep").write_text("\n", encoding="utf-8")
            for name in ("ca-bundle.crt", "permissions.cfg", "server.cfg"):
                (server / name).write_text("test\n", encoding="utf-8")
            for name in ("keys", "unique-items"):
                directory = server / "install_data" / name
                directory.mkdir(parents=True, exist_ok=True)
                (directory / ".keep").write_text("\n", encoding="utf-8")
            (server / "install_data" / "bans").write_text("", encoding="utf-8")
            (server / "install_data" / "motd").write_text(
                "Welcome\n", encoding="utf-8"
            )
        command("git", "add", ".", cwd=seed)
        command("git", "commit", "-m", "feat: seed", cwd=seed)
        command("git", "remote", "add", "origin", str(origin), cwd=seed)
        command("git", "push", "-u", "origin", checkout.branch, cwd=seed)
        command("git", "symbolic-ref", "HEAD", f"refs/heads/{checkout.branch}", cwd=origin)
        destination = self.wrapper / checkout.path
        command("git", "clone", str(origin), str(destination), cwd=self.root)
        return destination

    def make_scope_server_build(
        self,
        root: Path,
        profile: str,
        key: str,
        rendezvous: Path,
        marker: str,
    ) -> None:
        binary = root / "build" / "server"
        binary.mkdir(parents=True)
        executable = binary / "atrinik-server"
        executable.write_text(
            "#!/usr/bin/env python3\n"
            "import os, pathlib, socket, sys, time\n"
            "port = int(next(value.split('=', 1)[1] for value in sys.argv "
            "if value.startswith('--port_quic=')))\n"
            "datapath = pathlib.Path(next(value.split('=', 1)[1] for value in "
            "sys.argv if value.startswith('--datapath=')))\n"
            "(datapath / 'scope-state-proof').write_text('isolated\\n', "
            "encoding='utf-8')\n"
            f"rendezvous = pathlib.Path({str(rendezvous)!r})\n"
            f"marker = rendezvous / {marker!r}\n"
            "udp = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)\n"
            "udp.bind(('0.0.0.0', port))\n"
            "marker.write_text('bound\\n', encoding='utf-8')\n"
            "deadline = time.monotonic() + 10\n"
            "while len(list(rendezvous.glob('*.bound'))) != 2:\n"
            "    if time.monotonic() >= deadline:\n"
            "        raise RuntimeError('scope readiness rendezvous timed out')\n"
            "    time.sleep(0.01)\n"
            f"print('QUIC certificate SHA-256: {'d' * 64}', flush=True)\n"
            "print('Server ready. Waiting for connections...', flush=True)\n"
            "while True:\n"
            "    time.sleep(0.1)\n",
            encoding="utf-8",
        )
        executable.chmod(0o755)
        for name in ("libplugin_arena.so", "libplugin_python.so"):
            (binary / name).write_text("test\n", encoding="utf-8")
        for path, purpose in (
            (root / "runtime" / "content", "collected-content"),
            (root / "runtime" / "resources", "resource-view"),
            (root / "runtime" / "client-maps", "region-map-cache"),
        ):
            path.mkdir(parents=True, exist_ok=True)
            atomic_json(
                path / MANAGED_MARKER,
                {"schema_version": 1, "purpose": purpose},
            )
        (root / "runtime" / "content" / "lib").mkdir()
        (root / "runtime" / "content" / "maps").mkdir()
        (root / "runtime" / "client-maps" / "incuna_-1.png").write_bytes(
            b"\x89PNG\r\n\x1a\n"
        )
        (root / "runtime" / "client-maps" / "incuna_-1.def").write_text(
            "pixel_size 4\n", encoding="utf-8"
        )
        atomic_json(root / BUILD_METADATA, {"profile": profile, "key": key})
        atomic_json(
            root / MANAGED_MARKER,
            {"schema_version": 1, "purpose": f"profile:{profile}:{key}"},
        )

    def release_scope(self, name: str) -> dict[str, object]:
        preview = self.workspace.scope_release(name, apply=False)
        self.assertTrue(preview["can_apply"], preview["items"])
        return self.workspace.scope_release(
            name, apply=True, plan_sha256=preview["plan_sha256"]
        )

    def stop_topology_after_guardian_handoff(self, name: str) -> dict[str, object]:
        retry_gate = threading.Event()
        observations = 0
        while observations < 100:
            try:
                return self.workspace.topology_down(name, timeout=10)
            except locking_module.LockBusyError:
                status = self.workspace.topology_status(name)
                self.assertIn(
                    status["observation"]["process_tree_lease"],
                    {"retained", "released"},
                )
                observations += 1
                retry_gate.wait(0.01)
        self.fail(f"guardian did not release the exact state lease for {name}")

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

    def test_rolled_back_scope_retries_and_adopts_branch_only_side_effect(self) -> None:
        checkout = self.make_checkout("client")
        base = command("git", "rev-parse", "HEAD", cwd=checkout)
        branch = "issue/512-recovery"
        original = self.workspace._create_worktree

        def fail_after_branch(
            component_name: str,
            label: str,
            branch_name: str,
            start_point: str | None,
            existing: bool,
            *arguments: object,
            **keywords: object,
        ) -> Path:
            if not existing:
                command("git", "branch", branch_name, start_point or base, cwd=checkout)
                raise WorkspaceError("git-lfs filter unavailable")
            return original(
                component_name,
                label,
                branch_name,
                start_point,
                existing,
                *arguments,
                **keywords,
            )

        with mock.patch.object(
            self.workspace, "_create_worktree", side_effect=fail_after_branch
        ):
            with self.assertRaisesRegex(WorkspaceError, "git-lfs filter unavailable"):
                self.workspace.scope_create(
                    ["client"],
                    name="rolled-back",
                    branches=[f"client={branch}"],
                    start_points=[f"client={base}"],
                )

        scope_root = self.workspace_directory / "scopes" / "rolled-back"
        journal_path = scope_root / "creation-journal.json"
        journal = json.loads(journal_path.read_text(encoding="utf-8"))
        self.assertEqual(journal["status"], "rolled-back")
        self.assertEqual(journal["worktrees"][0]["status"], "planned")
        self.assertEqual(journal["request"]["worktrees"][0]["branch"], branch)
        self.assertEqual(
            journal["identities"]["repositories"][0]["path"], str(checkout)
        )
        legacy_journal = copy.deepcopy(journal)
        legacy_journal.pop("request")
        legacy_journal.pop("identities")
        journal_path.write_text(json.dumps(legacy_journal), encoding="utf-8")

        record = self.workspace.scope_create(
            ["client"],
            name="rolled-back",
            branches=[f"client={branch}"],
            start_points=[f"client={base}"],
        )
        self.assertEqual(record["status"], "complete")
        self.assertEqual(record["request_sha256"], journal["request_sha256"])
        self.assertEqual(self.workspace.scope_show("rolled-back"), record)
        recovered_journal = json.loads(journal_path.read_text(encoding="utf-8"))
        self.assertEqual(recovered_journal["status"], "complete")
        self.assertEqual(recovered_journal["recovery"]["status"], "complete")
        self.assertTrue(Path(record["worktrees"][0]["path"]).is_dir())

    def test_rolled_back_scope_recovery_rejects_changed_branch_and_existing_refs(self) -> None:
        checkout = self.make_checkout("client")
        base = command("git", "rev-parse", "HEAD", cwd=checkout)

        def leave_branch(name: str, branch: str) -> None:
            original = self.workspace._create_worktree

            def fail_after_branch(
                component_name: str,
                label: str,
                branch_name: str,
                start_point: str | None,
                existing: bool,
                *arguments: object,
                **keywords: object,
            ) -> Path:
                if not existing:
                    command("git", "branch", branch_name, start_point or base, cwd=checkout)
                    raise WorkspaceError("simulated filter failure")
                return original(
                    component_name,
                    label,
                    branch_name,
                    start_point,
                    existing,
                    *arguments,
                    **keywords,
                )

            with mock.patch.object(
                self.workspace, "_create_worktree", side_effect=fail_after_branch
            ):
                with self.assertRaisesRegex(WorkspaceError, "simulated filter failure"):
                    self.workspace.scope_create(
                        ["client"],
                        name=name,
                        branches=[f"client={branch}"],
                        start_points=[f"client={base}"],
                    )

        leave_branch("changed-branch", "issue/512-changed")
        changed = checkout / "changed.txt"
        changed.write_text("changed\n", encoding="utf-8")
        command("git", "add", "changed.txt", cwd=checkout)
        command("git", "commit", "-m", "test: advance branch", cwd=checkout)
        command("git", "branch", "-f", "issue/512-changed", "HEAD", cwd=checkout)
        with self.assertRaisesRegex(WorkspaceError, "branch changed"):
            self.workspace.scope_create(
                ["client"],
                name="changed-branch",
                branches=["client=issue/512-changed"],
                start_points=[f"client={base}"],
            )

        leave_branch("dirty-primary", "issue/512-dirty-primary")
        (checkout / "dirty.txt").write_text("dirty\n", encoding="utf-8")
        with self.assertRaisesRegex(WorkspaceError, "primary checkout is dirty"):
            self.workspace.scope_create(
                ["client"],
                name="dirty-primary",
                branches=["client=issue/512-dirty-primary"],
                start_points=[f"client={base}"],
            )
        (checkout / "dirty.txt").unlink()

        leave_branch("profile-reference", "issue/512-profile")
        profile = self.workspace_directory / "profiles" / "scope-profile-reference.json"
        profile.write_text("{}", encoding="utf-8")
        with self.assertRaisesRegex(WorkspaceError, "profile already exists"):
            self.workspace.scope_create(
                ["client"],
                name="profile-reference",
                branches=["client=issue/512-profile"],
                start_points=[f"client={base}"],
            )

        leave_branch("topology-reference", "issue/512-topology")
        (self.workspace_directory / "topologies" / "scope-topology-reference").mkdir(
            parents=True
        )
        with self.assertRaisesRegex(WorkspaceError, "topology namespace"):
            self.workspace.scope_create(
                ["client"],
                name="topology-reference",
                branches=["client=issue/512-topology"],
                start_points=[f"client={base}"],
            )

        leave_branch("remote-reference", "issue/512-remote")
        command(
            "git",
            "update-ref",
            "refs/remotes/origin/issue/512-remote",
            base,
            cwd=checkout,
        )
        with self.assertRaisesRegex(WorkspaceError, "remote branch reference"):
            self.workspace.scope_create(
                ["client"],
                name="remote-reference",
                branches=["client=issue/512-remote"],
                start_points=[f"client={base}"],
            )

        leave_branch("row-drift", "issue/512-row-drift")
        journal_path = (
            self.workspace_directory / "scopes" / "row-drift" / "creation-journal.json"
        )
        journal = json.loads(journal_path.read_text(encoding="utf-8"))
        journal["worktrees"][0]["branch"] = "issue/512-tampered"
        journal_path.write_text(json.dumps(journal), encoding="utf-8")
        with self.assertRaisesRegex(WorkspaceError, "request evidence changed"):
            self.workspace.scope_create(
                ["client"],
                name="row-drift",
                branches=["client=issue/512-row-drift"],
                start_points=[f"client={base}"],
            )
        self.assertFalse(
            (self.workspace_directory / "worktrees" / "client" / "scope-row-drift").exists()
        )

    def test_rolled_back_scope_recovery_rejects_existing_worktree_and_serializes_retries(self) -> None:
        checkout = self.make_checkout("client")
        base = command("git", "rev-parse", "HEAD", cwd=checkout)

        def leave_branch(name: str, branch: str) -> None:
            original = self.workspace._create_worktree

            def fail_after_branch(
                component_name: str,
                label: str,
                branch_name: str,
                start_point: str | None,
                existing: bool,
                *arguments: object,
                **keywords: object,
            ) -> Path:
                if not existing:
                    command("git", "branch", branch_name, start_point or base, cwd=checkout)
                    raise WorkspaceError("simulated missing filter")
                return original(
                    component_name,
                    label,
                    branch_name,
                    start_point,
                    existing,
                    *arguments,
                    **keywords,
                )

            with mock.patch.object(
                self.workspace, "_create_worktree", side_effect=fail_after_branch
            ):
                with self.assertRaisesRegex(WorkspaceError, "simulated missing filter"):
                    self.workspace.scope_create(
                        ["client"],
                        name=name,
                        branches=[f"client={branch}"],
                        start_points=[f"client={base}"],
                    )

        leave_branch("worktree-reference", "issue/512-worktree")
        destination = (
            self.workspace_directory / "worktrees" / "client" / "scope-worktree-reference"
        )
        command("git", "worktree", "add", str(destination), "issue/512-worktree", cwd=checkout)
        (destination / "dirty.txt").write_text("dirty\n", encoding="utf-8")
        with self.assertRaisesRegex(WorkspaceError, "worktree path"):
            self.workspace.scope_create(
                ["client"],
                name="worktree-reference",
                branches=["client=issue/512-worktree"],
                start_points=[f"client={base}"],
            )

        leave_branch("concurrent-retry", "issue/512-concurrent")
        barrier = threading.Barrier(2)

        def retry() -> tuple[str, object]:
            barrier.wait(timeout=10)
            try:
                return (
                    "ok",
                    self.workspace.scope_create(
                        ["client"],
                        name="concurrent-retry",
                        branches=["client=issue/512-concurrent"],
                        start_points=[f"client={base}"],
                    ),
                )
            except WorkspaceError as error:
                return ("error", str(error))

        with ThreadPoolExecutor(max_workers=2) as workers:
            results = list(workers.map(lambda _: retry(), range(2)))
        self.assertEqual(sorted(result[0] for result in results), ["error", "ok"])
        completed = next(result[1] for result in results if result[0] == "ok")
        self.assertEqual(completed["status"], "complete")
        self.assertIn("already in use", next(result[1] for result in results if result[0] == "error"))
        self.assertEqual(
            self.workspace.scope_create(
                ["client"],
                name="concurrent-retry",
                branches=["client=issue/512-concurrent"],
                start_points=[f"client={base}"],
            ),
            completed,
        )

    def test_rolled_back_scope_without_branch_side_effect_retries_exactly(self) -> None:
        self.make_checkout("client")
        with mock.patch.dict(
            os.environ, {SCOPE_FAILURE_BOUNDARIES_ENV: "worktree:client"}
        ):
            with self.assertRaisesRegex(WorkspaceError, "injected scope failure"):
                self.workspace.scope_create(["client"], name="retry-rollback")
        journal = json.loads(
            (
                self.workspace_directory
                / "scopes"
                / "retry-rollback"
                / "creation-journal.json"
            ).read_text(encoding="utf-8")
        )
        self.assertEqual(journal["status"], "rolled-back")
        self.assertEqual(
            self.workspace.scope_create(["client"], name="retry-rollback")["status"],
            "complete",
        )

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
        topology = self.workspace_directory / "topologies" / "scope-topology-exists"
        topology.mkdir(parents=True)
        with self.assertRaisesRegex(WorkspaceError, "topology namespace"):
            preflight("topology-exists")
        with self.assertRaisesRegex(WorkspaceError, "must be canonical"):
            preflight("noncanonical-topology", topology="noncanonical-topology")
        canonical = preflight("canonical-contract")
        self.assertEqual(canonical["profile"]["name"], "scope-canonical-contract")
        self.assertEqual(canonical["topology"]["name"], "scope-canonical-contract")
        self.assertEqual(
            canonical["topology"]["path"],
            str(self.workspace_directory / "topologies" / "scope-canonical-contract"),
        )
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

    def test_complete_dual_scope_server_lifecycle_is_independent(self) -> None:
        self.make_checkout("classic")
        self.make_checkout("content")
        self.make_checkout("resources")
        scopes = [
            self.workspace.scope_create(
                ["classic-server"], name=f"complete-{suffix}", base_profile="classic"
            )
            for suffix in ("a", "b")
        ]
        self.assertEqual(
            len({record["worktrees"][0]["path"] for record in scopes}), 2
        )
        self.assertEqual(
            len({record["worktrees"][0]["common_git_dir"] for record in scopes}),
            1,
        )

        rendezvous = self.root / "complete-scope-readiness"
        rendezvous.mkdir()
        roots: dict[str, Path] = {}
        for index, record in enumerate(scopes):
            profile = record["profile"]["name"]
            key = str(index + 1) * 64
            root = self.workspace.paths.builds / "profiles" / f"{profile}-{key}"
            self.make_scope_server_build(
                root, profile, key, rendezvous, f"{record['name']}.bound"
            )
            roots[profile] = root

        build_rendezvous = threading.Barrier(2)
        coordination: list[tuple[str, str]] = []
        coordination_lock = threading.Lock()
        build_calls: dict[str, int] = {}
        first_build_holds_generation = threading.Event()

        def controlled_build(
            workspace: Workspace,
            target: str,
            profile: str,
            tests: bool,
            targets: list[str],
            selected: dict[str, Path],
            **_options: object,
        ) -> Path:
            self.assertIn("server", selected)
            with coordination_lock:
                phase = "build" if build_calls.get(profile, 0) == 0 else "startup-build"
                build_calls[profile] = build_calls.get(profile, 0) + 1
                coordination.append((phase, profile))
            if phase == "build":
                self.assertEqual(target, "server")
                self.assertTrue(tests)
                if profile == scopes[0]["profile"]["name"]:
                    first_build_holds_generation.set()
            else:
                self.assertEqual(target, "topology")
                self.assertFalse(tests)
            build_rendezvous.wait(timeout=10)
            return roots[profile]

        sessions = [Workspace(self.wrapper), Workspace(self.wrapper)]
        names = [record["topology"]["name"] for record in scopes]
        try:
            with mock.patch.object(Workspace, "_build_resolved", controlled_build):
                with ThreadPoolExecutor(max_workers=2) as executor:
                    builds = [
                        executor.submit(
                            sessions[0].build,
                            "server",
                            scopes[0]["profile"]["name"],
                            True,
                        )
                    ]
                    self.assertTrue(first_build_holds_generation.wait(timeout=10))
                    builds.append(
                        executor.submit(
                            sessions[1].build,
                            "server",
                            scopes[1]["profile"]["name"],
                            True,
                        )
                    )
                    self.assertEqual(
                        {future.result(timeout=20) for future in builds}, set(roots.values())
                    )
                with ThreadPoolExecutor(max_workers=2) as executor:
                    startups = [
                        executor.submit(
                            sessions[index].topology_up,
                            names[index],
                            scopes[index]["profile"]["name"],
                            None,
                            ["server"],
                            None,
                            "temporary",
                        )
                        for index in range(2)
                    ]
                    statuses = [future.result(timeout=30) for future in startups]

            self.assertEqual(
                coordination.count(("build", scopes[0]["profile"]["name"])), 1
            )
            self.assertEqual(
                coordination.count(("build", scopes[1]["profile"]["name"])), 1
            )
            self.assertEqual(
                coordination.count(
                    ("startup-build", scopes[0]["profile"]["name"])
                ),
                1,
            )
            self.assertEqual(
                coordination.count(
                    ("startup-build", scopes[1]["profile"]["name"])
                ),
                1,
            )
            self.assertEqual(
                {path.name for path in rendezvous.glob("*.bound")},
                {f"{record['name']}.bound" for record in scopes},
            )
            self.assertTrue(all(status["ready"] for status in statuses))
            self.assertEqual(
                len({status["endpoint"]["port"] for status in statuses}), 2
            )
            self.assertEqual(
                len({status["state_policy"]["path"] for status in statuses}), 2
            )

            observed = [self.workspace.topology_status(name) for name in names]
            a_generation = observed[0]["runtime"]["generation"]
            a_manifest = observed[0]["runtime"]["manifest_sha256"]
            self.assertTrue(observed[0]["supervisor"]["running"])
            self.assertTrue(observed[1]["supervisor"]["running"])

            stopped_b = self.stop_topology_after_guardian_handoff(names[1])
            self.assertFalse(stopped_b["supervisor"]["running"])
            spec_path = Path(scopes[1]["topology"]["path"]) / "spec.json"
            status_path = Path(scopes[1]["topology"]["path"]) / "status.json"
            spec = json.loads(spec_path.read_text(encoding="utf-8"))
            persisted = json.loads(status_path.read_text(encoding="utf-8"))
            divergent = copy.deepcopy(spec)
            coordinate = next(iter(divergent["resolved"].values()))
            coordinate["checkout_path"] = scopes[0]["worktrees"][0]["path"]
            atomic_json(spec_path, divergent)
            divergent_preview = self.workspace.scope_release(
                scopes[1]["name"], apply=False
            )
            self.assertIn(
                "unproven_clean_topology_stop",
                next(
                    item
                    for item in divergent_preview["items"]
                    if item["kind"] == "topology"
                )["reasons"],
            )
            atomic_json(spec_path, spec)
            coherent_spec = copy.deepcopy(spec)
            coherent_status = copy.deepcopy(persisted)
            coherent_observed = self.workspace.topology_status(names[1])
            for topology_record in (coherent_spec, coherent_status):
                coordinate = next(iter(topology_record["resolved"].values()))
                coordinate["head"] = "f" * 40
            for coordinate in coherent_observed["resolved"].values():
                if coordinate["checkout"] == scopes[1]["worktrees"][0]["checkout"]:
                    coordinate["head"] = "f" * 40
            atomic_json(spec_path, coherent_spec)
            atomic_json(status_path, coherent_status)
            with mock.patch.object(
                self.workspace,
                "topology_status",
                return_value=coherent_observed,
            ):
                coherent_preview = self.workspace.scope_release(
                    scopes[1]["name"], apply=False
                )
            self.assertIn(
                "mismatched_topology_records",
                next(
                    item
                    for item in coherent_preview["items"]
                    if item["kind"] == "topology"
                )["reasons"],
            )
            atomic_json(spec_path, spec)
            atomic_json(status_path, persisted)
            race_preview = self.workspace.scope_release(
                scopes[1]["name"], apply=False
            )
            replaced = False

            def replace_after_plan(boundary: str) -> None:
                nonlocal replaced
                if boundary == "release:journal" and not replaced:
                    replaced = True
                    atomic_json(spec_path, coherent_spec)

            with mock.patch.object(
                ScopeLifecycle, "_maybe_fail", side_effect=replace_after_plan
            ):
                with self.assertRaisesRegex(
                    WorkspaceError, "topology evidence changed"
                ):
                    self.workspace.scope_release(
                        scopes[1]["name"],
                        apply=True,
                        plan_sha256=race_preview["plan_sha256"],
                    )
            self.assertTrue(Path(scopes[1]["worktrees"][0]["path"]).is_dir())
            self.assertTrue(Path(scopes[1]["profile"]["path"]).is_file())
            atomic_json(spec_path, spec)
            released_b = self.release_scope(scopes[1]["name"])
            self.assertTrue(released_b["released"])

            live_a = self.workspace.topology_status(names[0])
            self.assertTrue(live_a["supervisor"]["running"])
            self.assertEqual(live_a["runtime"]["generation"], a_generation)
            self.assertEqual(live_a["runtime"]["manifest_sha256"], a_manifest)
            self.assertTrue(Path(scopes[0]["worktrees"][0]["path"]).is_dir())
            self.assertTrue(Path(scopes[0]["profile"]["path"]).is_file())
            self.assertFalse(Path(scopes[1]["worktrees"][0]["path"]).exists())
            self.assertFalse(Path(scopes[1]["profile"]["path"]).exists())

            stopped_a = self.stop_topology_after_guardian_handoff(names[0])
            self.assertFalse(stopped_a["supervisor"]["running"])
            released_a = self.release_scope(scopes[0]["name"])
            self.assertTrue(released_a["released"])
        finally:
            for name in names:
                try:
                    status = self.workspace.topology_status(name)
                    if status["supervisor"]["running"] or any(
                        service["running"] for service in status["services"].values()
                    ):
                        self.stop_topology_after_guardian_handoff(name)
                except (OSError, WorkspaceError):
                    pass
            for session in sessions:
                session.close()

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

    def test_classic_logical_selector_uses_physical_override_namespace(self) -> None:
        checkout = self.make_checkout("classic")
        start = command("git", "rev-parse", "HEAD", cwd=checkout)
        record = self.workspace.scope_create(
            ["classic-client"],
            name="classic-logical-selector",
            base_profile="classic",
            labels=["classic=classic-logical-label"],
            branches=["classic=fix/classic-logical-selector"],
            start_points=[f"classic={start}"],
        )
        self.assertEqual(record["requested_components"], ["classic-client"])
        self.assertEqual(record["worktrees"][0]["checkout"], "classic")
        self.assertEqual(record["worktrees"][0]["label"], "classic-logical-label")
        with self.assertRaisesRegex(WorkspaceError, "unselected checkouts"):
            self.workspace.scope_create(
                ["classic-client"],
                name="classic-logical-selector-rejected",
                base_profile="classic",
                labels=["classic-client=wrong-namespace"],
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

    def test_release_accepts_plan_bound_clean_descendant_and_cas_deletes_branch(self) -> None:
        self.make_checkout("client")
        record = self.workspace.scope_create(["client"], name="descendant-release")
        worktree = Path(record["worktrees"][0]["path"])
        (worktree / "committed.txt").write_text("committed\n", encoding="utf-8")
        subprocess.run(["git", "add", "committed.txt"], cwd=worktree, check=True)
        subprocess.run(
            [
                "git",
                "-c",
                "user.name=Atrinik Tests",
                "-c",
                "user.email=tests@atrinik.org",
                "commit",
                "-m",
                "test: descendant",
            ],
            cwd=worktree,
            check=True,
        )
        descendant = subprocess.run(
            ["git", "rev-parse", "HEAD"],
            cwd=worktree,
            check=True,
            capture_output=True,
            text=True,
        ).stdout.strip()

        preview = self.workspace.scope_release("descendant-release", apply=False)
        item = next(row for row in preview["items"] if row["kind"] == "worktree")
        self.assertEqual(item["branch_head"], descendant)
        result = self.workspace.scope_release(
            "descendant-release", apply=True, plan_sha256=preview["plan_sha256"]
        )
        self.assertTrue(result["released"])
        branch = subprocess.run(
            ["git", "for-each-ref", "--format=%(objectname)", f"refs/heads/{record['worktrees'][0]['branch']}"],
            cwd=Path(record["worktrees"][0]["primary_path"]),
            check=True,
            capture_output=True,
            text=True,
        ).stdout.strip()
        self.assertEqual(branch, "")

    def test_release_retries_branch_cas_after_worktree_removal(self) -> None:
        self.make_checkout("client")
        record = self.workspace.scope_create(["client"], name="branch-cas-retry")
        preview = self.workspace.scope_release("branch-cas-retry", apply=False)
        with mock.patch.dict(
            os.environ,
            {scopes_module.SCOPE_FAILURE_BOUNDARIES_ENV: "release:worktree-path:client"},
        ):
            with self.assertRaisesRegex(WorkspaceError, "injected scope failure"):
                self.workspace.scope_release(
                    "branch-cas-retry",
                    apply=True,
                    plan_sha256=preview["plan_sha256"],
                )
        result = self.workspace.scope_release(
            "branch-cas-retry", apply=True, plan_sha256=preview["plan_sha256"]
        )
        self.assertTrue(result["released"])
        self.assertFalse(Path(record["worktrees"][0]["path"]).exists())

    def test_release_cas_deletes_branch_for_already_absent_worktree(self) -> None:
        self.make_checkout("client")
        record = self.workspace.scope_create(["client"], name="branch-only-release")
        row = record["worktrees"][0]
        subprocess.run(
            ["git", "worktree", "remove", row["path"]],
            cwd=row["primary_path"],
            check=True,
        )

        preview = self.workspace.scope_release("branch-only-release", apply=False)
        item = next(row for row in preview["items"] if row["kind"] == "worktree")
        self.assertEqual(item["disposition"], "eligible")
        self.assertEqual(item["reasons"], ["worktree_removed_branch_pending"])
        result = self.workspace.scope_release(
            "branch-only-release",
            apply=True,
            plan_sha256=preview["plan_sha256"],
        )

        self.assertTrue(result["released"])
        self.assertEqual(
            command(
                "git",
                "for-each-ref",
                "--format=%(objectname)",
                f"refs/heads/{row['branch']}",
                cwd=Path(row["primary_path"]),
            ),
            "",
        )
        journal = json.loads(Path(result["journal"]).read_text(encoding="utf-8"))
        self.assertIn("worktree:client", journal["completed"])

    def test_release_retries_absent_worktree_branch_cas_after_unlink(self) -> None:
        self.make_checkout("client")
        record = self.workspace.scope_create(["client"], name="branch-only-retry")
        row = record["worktrees"][0]
        subprocess.run(
            ["git", "worktree", "remove", row["path"]],
            cwd=row["primary_path"],
            check=True,
        )
        preview = self.workspace.scope_release("branch-only-retry", apply=False)
        with mock.patch.dict(
            os.environ,
            {
                scopes_module.SCOPE_FAILURE_BOUNDARIES_ENV:
                "release:worktree-branch:client"
            },
        ):
            with self.assertRaisesRegex(WorkspaceError, "injected scope failure"):
                self.workspace.scope_release(
                    "branch-only-retry",
                    apply=True,
                    plan_sha256=preview["plan_sha256"],
                )
        release_path = (
            self.workspace_directory
            / "scopes"
            / "branch-only-retry"
            / "release-journal.json"
        )
        interrupted = json.loads(release_path.read_text(encoding="utf-8"))
        self.assertEqual(
            interrupted["in_flight"],
            {"action": "worktree:client", "phase": "removing"},
        )
        result = self.workspace.scope_release(
            "branch-only-retry",
            apply=True,
            plan_sha256=preview["plan_sha256"],
        )
        self.assertTrue(result["released"])
        self.assertEqual(
            command(
                "git",
                "for-each-ref",
                "--format=%(objectname)",
                f"refs/heads/{row['branch']}",
                cwd=Path(row["primary_path"]),
            ),
            "",
        )

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

    def test_release_rejects_malformed_journal_coordinates(self) -> None:
        self.make_checkout("client")
        record = self.workspace.scope_create(["client"], name="invalid-journal")
        lifecycle = ScopeLifecycle(self.workspace)
        preview = self.workspace.scope_release("invalid-journal", apply=False)
        release_path = Path(record["cleanup"]["release_journal"])

        self.assertEqual(
            self.workspace._scope_release_live_plan("invalid-journal")[
                "plan_sha256"
            ],
            preview["plan_sha256"],
        )
        worktree = record["worktrees"][0]
        self.assertTrue(
            self.workspace._worktree_path_registered(
                Path(worktree["primary_path"]), Path(worktree["path"])
            )
        )

        with self.assertRaisesRegex(WorkspaceError, "build path is invalid"):
            lifecycle._release_build_root(record, None)
        with self.assertRaisesRegex(WorkspaceError, "build path is invalid"):
            lifecycle._release_build_root(record, str(release_path))
        with self.assertRaisesRegex(WorkspaceError, "journal plan is invalid"):
            lifecycle._journal_build_roots(record, {"plan": None})
        with self.assertRaisesRegex(WorkspaceError, "journal plan is invalid"):
            lifecycle._journal_build_roots(record, {"plan": {"items": [None]}})
        with self.assertRaisesRegex(WorkspaceError, "journal is invalid"):
            lifecycle._journal_build_roots(
                record, {"plan": {"items": []}, "pending_builds": {}}
            )
        with self.assertRaisesRegex(WorkspaceError, "journal is invalid"):
            lifecycle._journal_build_roots(
                record, {"plan": {"items": []}, "pending_builds": [None]}
            )

        release_path.symlink_to(record["profile"]["path"])
        with self.assertRaisesRegex(WorkspaceError, "journal is unsafe"):
            lifecycle._release_journal_build_roots(record)
        with mock.patch.object(
            ScopeLifecycle, "_release_journal_build_roots", return_value=set()
        ):
            with self.assertRaisesRegex(WorkspaceError, "journal is unsafe"):
                self.workspace.scope_release("invalid-journal", apply=False)
        release_path.unlink()

        scopes_module.durable_atomic_json(release_path, {})
        with self.assertRaisesRegex(WorkspaceError, "journal plan is invalid"):
            lifecycle._release_journal_build_roots(record)
        with mock.patch.object(
            ScopeLifecycle, "_release_journal_build_roots", return_value=set()
        ):
            with self.assertRaisesRegex(WorkspaceError, "journal plan is invalid"):
                self.workspace.scope_release("invalid-journal", apply=False)

        fake_build = (
            self.workspace.paths.builds
            / "profiles"
            / f"{record['profile']['name']}-{'f' * 64}"
        )
        changed_plan = {
            "schema_version": preview["schema_version"],
            "scope": record["name"],
            "generation": record["generation"],
            "items": [
                {
                    "kind": "build",
                    "disposition": "eligible",
                    "path": str(fake_build),
                }
            ],
        }
        scopes_module.durable_atomic_json(
            release_path,
            {
                "plan": changed_plan,
                "plan_sha256": scopes_module._canonical_sha256(changed_plan),
            },
        )
        with mock.patch.object(
            ScopeLifecycle, "_release_journal_build_roots", return_value=set()
        ):
            with self.assertRaisesRegex(
                WorkspaceError, "journal build coordinates changed"
            ):
                self.workspace.scope_release("invalid-journal", apply=False)

        retained_plan = {
            key: copy.deepcopy(preview[key])
            for key in ("schema_version", "scope", "generation", "items")
        }
        scopes_module.durable_atomic_json(
            release_path,
            {
                "schema_version": 1,
                "scope": record["name"],
                "generation": record["generation"],
                "plan_sha256": preview["plan_sha256"],
                "plan": retained_plan,
                "status": "applying",
                "completed": ["profile"],
                "in_flight": None,
                "pending_builds": [],
                "updated_at": "2026-08-14T00:00:00Z",
            },
        )
        with self.assertRaisesRegex(WorkspaceError, "journal plan is invalid"):
            self.workspace.scope_release(
                "invalid-journal", apply=True, plan_sha256="f" * 64
            )

    def test_pristine_release_journal_can_replan_after_pre_action_drift(self) -> None:
        self.make_checkout("client")
        record = self.workspace.scope_create(["client"], name="pristine-replan")
        preview = self.workspace.scope_release("pristine-replan", apply=False)
        stale_plan = {
            key: copy.deepcopy(preview[key])
            for key in ("schema_version", "scope", "generation", "items")
        }
        topology = next(
            item for item in stale_plan["items"] if item["kind"] == "topology"
        )
        topology["reasons"] = ["stale-pre-action-observation"]
        stale_digest = scopes_module._canonical_sha256(stale_plan)
        release_path = (
            self.workspace_directory
            / "scopes"
            / "pristine-replan"
            / "release-journal.json"
        )
        scopes_module.durable_atomic_json(
            release_path,
            {
                "schema_version": 1,
                "scope": "pristine-replan",
                "generation": record["generation"],
                "plan_sha256": stale_digest,
                "plan": stale_plan,
                "status": "applying",
                "completed": [],
                "in_flight": None,
                "pending_builds": [],
                "updated_at": "2026-08-14T00:00:00Z",
            },
        )

        replanned = self.workspace.scope_release("pristine-replan", apply=False)
        self.assertEqual(replanned["plan_sha256"], preview["plan_sha256"])
        self.assertNotEqual(replanned["plan_sha256"], stale_digest)
        result = self.workspace.scope_release(
            "pristine-replan",
            apply=True,
            plan_sha256=replanned["plan_sha256"],
        )
        self.assertTrue(result["released"])
        journal = json.loads(release_path.read_text(encoding="utf-8"))
        self.assertEqual(journal["plan_sha256"], replanned["plan_sha256"])
        self.assertEqual(journal["status"], "complete")

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

    def test_release_recovers_interruptions_inside_owned_build_removal(self) -> None:
        self.make_checkout("client")
        from atrinik_workspace import workspace as workspace_module

        for boundary in ("child", "root"):
            with self.subTest(boundary=boundary):
                name = f"inside-build-{boundary}"
                record = self.workspace.scope_create(["client"], name=name)
                key = ("c" if boundary == "child" else "d") * 64
                build_root = (
                    self.workspace.paths.builds
                    / "profiles"
                    / f"{record['profile']['name']}-{key}"
                )
                build_root.mkdir(parents=True)
                atomic_json(
                    build_root / BUILD_METADATA,
                    {"profile": record["profile"]["name"], "key": key},
                )
                atomic_json(
                    build_root / MANAGED_MARKER,
                    {
                        "schema_version": 1,
                        "purpose": f"profile:{record['profile']['name']}:{key}",
                    },
                )
                plan = self.workspace.scope_release(name, apply=False)
                if boundary == "child":
                    original = workspace_module.os.unlink
                    killed = False

                    def interrupt(
                        path: object, *args: object, **kwargs: object
                    ) -> None:
                        nonlocal killed
                        original(path, *args, **kwargs)
                        if (
                            not killed
                            and isinstance(path, str)
                            and path.startswith(".remove-")
                        ):
                            killed = True
                            raise SystemExit("inside child removal")

                    patcher = mock.patch.object(
                        workspace_module.os, "unlink", side_effect=interrupt
                    )
                else:
                    original_rename = workspace_module.rename_no_replace_at

                    def interrupt_rename(
                        *args: object, **kwargs: object
                    ) -> None:
                        original_rename(*args, **kwargs)
                        if args[1] == build_root.name:
                            raise SystemExit("after root tombstone")

                    patcher = mock.patch.object(
                        workspace_module,
                        "rename_no_replace_at",
                        side_effect=interrupt_rename,
                    )
                with patcher:
                    with self.assertRaises(SystemExit):
                        self.workspace.scope_release(
                            name,
                            apply=True,
                            plan_sha256=plan["plan_sha256"],
                        )

                result = self.workspace.scope_release(
                    name, apply=True, plan_sha256=plan["plan_sha256"]
                )
                self.assertTrue(result["released"])
                self.assertFalse(build_root.exists())
                self.assertFalse(
                    any(
                        path.name.startswith(".remove-")
                        for path in build_root.parent.iterdir()
                    )
                )

    def test_release_reacquires_journal_build_lock_after_root_removal(self) -> None:
        self.make_checkout("client")
        record = self.workspace.scope_create(["client"], name="build-lock-retry")
        key = "9" * 64
        build_root = (
            self.workspace.paths.builds
            / "profiles"
            / f"{record['profile']['name']}-{key}"
        )
        build_root.mkdir(parents=True)
        atomic_json(
            build_root / BUILD_METADATA,
            {"profile": record["profile"]["name"], "key": key},
        )
        atomic_json(
            build_root / MANAGED_MARKER,
            {
                "schema_version": 1,
                "purpose": f"profile:{record['profile']['name']}:{key}",
            },
        )
        plan = self.workspace.scope_release("build-lock-retry", apply=False)
        with mock.patch.dict(
            os.environ,
            {SCOPE_FAILURE_BOUNDARIES_ENV: f"release:build-tree:{build_root.name}"},
        ):
            with self.assertRaisesRegex(WorkspaceError, "injected scope failure"):
                self.workspace.scope_release(
                    "build-lock-retry",
                    apply=True,
                    plan_sha256=plan["plan_sha256"],
                )
        self.assertFalse(build_root.exists())

        lock_path = self.workspace.paths.builds / "locks" / f"{build_root.name}.lock"
        with locking_module.exclusive_lock(lock_path, "concurrent build recreation"):
            with mock.patch.object(
                ScopeLifecycle, "_candidate_build_roots", return_value=[]
            ):
                with self.assertRaisesRegex(
                    WorkspaceError, "refused active resource leases"
                ):
                    self.workspace.scope_release(
                        "build-lock-retry",
                        apply=True,
                        plan_sha256=plan["plan_sha256"],
                    )

        journal = json.loads(
            Path(record["cleanup"]["release_journal"]).read_text(encoding="utf-8")
        )
        self.assertEqual(journal["status"], "applying")
        self.assertEqual(journal["completed"], [])
        self.assertEqual(journal["in_flight"]["phase"], "removing")
        result = self.workspace.scope_release(
            "build-lock-retry", apply=True, plan_sha256=plan["plan_sha256"]
        )
        self.assertTrue(result["released"])

    def test_prepared_scope_build_absence_is_not_credited_as_removal(self) -> None:
        self.make_checkout("client")
        record = self.workspace.scope_create(["client"], name="prepared-build")
        key = "e" * 64
        build_root = (
            self.workspace.paths.builds
            / "profiles"
            / f"{record['profile']['name']}-{key}"
        )
        build_root.mkdir(parents=True)
        atomic_json(
            build_root / BUILD_METADATA,
            {"profile": record["profile"]["name"], "key": key},
        )
        atomic_json(
            build_root / MANAGED_MARKER,
            {
                "schema_version": 1,
                "purpose": f"profile:{record['profile']['name']}:{key}",
            },
        )
        plan = self.workspace.scope_release("prepared-build", apply=False)
        real_sha256 = scopes_module._file_sha256
        metadata_reads = 0

        def interrupt(path: Path) -> str:
            nonlocal metadata_reads
            if path == build_root / BUILD_METADATA:
                metadata_reads += 1
                if metadata_reads == 2:
                    raise SystemExit("before build removal validation")
            return real_sha256(path)

        with mock.patch.object(scopes_module, "_file_sha256", side_effect=interrupt):
            with self.assertRaisesRegex(SystemExit, "before build removal validation"):
                self.workspace.scope_release(
                    "prepared-build",
                    apply=True,
                    plan_sha256=plan["plan_sha256"],
                )

        journal_path = Path(record["cleanup"]["release_journal"])
        prepared = json.loads(journal_path.read_text(encoding="utf-8"))
        self.assertEqual(
            prepared["in_flight"],
            {"action": f"build:{build_root}", "phase": "prepared"},
        )
        shutil.rmtree(build_root)

        with self.assertRaisesRegex(
            WorkspaceError, "prepared scope build disappeared before removal"
        ):
            self.workspace.scope_release(
                "prepared-build",
                apply=True,
                plan_sha256=plan["plan_sha256"],
            )
        retained = json.loads(journal_path.read_text(encoding="utf-8"))
        self.assertEqual(retained["completed"], [])
        self.assertEqual(retained["in_flight"]["phase"], "prepared")

    def test_every_release_publication_boundary_recovers_from_interruption(self) -> None:
        self.make_checkout("client")
        for index, boundary_kind in enumerate(
            (
                "journal",
                "build-tree",
                "build",
                "profile-file",
                "profile",
                "worktree-path",
                "worktree",
                "complete",
            )
        ):
            name = f"release-boundary-{index}"
            record = self.workspace.scope_create(["client"], name=name)
            profile = record["profile"]["name"]
            key = str(index + 1) * 64
            root = self.workspace.paths.builds / "profiles" / f"{profile}-{key}"
            root.mkdir(parents=True)
            atomic_json(root / BUILD_METADATA, {"profile": profile, "key": key})
            atomic_json(
                root / MANAGED_MARKER,
                {"schema_version": 1, "purpose": f"profile:{profile}:{key}"},
            )
            expected = {
                "journal": "release:journal",
                "build-tree": f"release:build-tree:{root.name}",
                "build": f"release:build:{root.name}",
                "profile-file": "release:profile-file",
                "profile": "release:profile",
                "worktree-path": "release:worktree-path:client",
                "worktree": "release:worktree:client",
                "complete": "release:complete",
            }[boundary_kind]
            preview = self.workspace.scope_release(name, apply=False)
            observed: list[str] = []

            def interrupt(boundary: str) -> None:
                observed.append(boundary)
                if boundary == expected:
                    raise WorkspaceError(
                        f"injected release failure after publication boundary: {boundary}"
                    )

            with self.subTest(boundary=expected), mock.patch.object(
                ScopeLifecycle, "_maybe_fail", side_effect=interrupt
            ):
                with self.assertRaisesRegex(
                    WorkspaceError, "injected release failure"
                ):
                    self.workspace.scope_release(
                        name,
                        apply=True,
                        plan_sha256=preview["plan_sha256"],
                    )
            self.assertIn(expected, observed)
            retry = self.workspace.scope_release(name, apply=False)
            released = self.workspace.scope_release(
                name, apply=True, plan_sha256=retry["plan_sha256"]
            )
            self.assertTrue(released["released"])
            journal = json.loads(
                Path(record["cleanup"]["release_journal"]).read_text(
                    encoding="utf-8"
                )
            )
            self.assertEqual(journal["status"], "complete")
            self.assertIn(f"build:{root}", journal["completed"])
            self.assertFalse(root.exists())
            self.assertFalse(Path(record["profile"]["path"]).exists())
            self.assertFalse(Path(record["worktrees"][0]["path"]).exists())
            self.assertFalse(
                any(
                    reference.get("reference") == profile
                    for reference in self.workspace._physical_reference_records()
                )
            )
            self.assertEqual(
                command(
                    "git",
                    "for-each-ref",
                    "--format=%(objectname)",
                    f"refs/heads/{record['worktrees'][0]['branch']}",
                    cwd=Path(record["worktrees"][0]["primary_path"]),
                ),
                "",
            )

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

    def test_clean_subset_topology_does_not_bind_unresolved_scope_worktree(
        self,
    ) -> None:
        self.make_checkout("client")
        self.make_checkout("server")
        record = self.workspace.scope_create(
            ["client", "server"], name="subset-topology"
        )
        rows = {row["checkout"]: row for row in record["worktrees"]}
        topology_root = Path(record["topology"]["path"])
        topology_root.mkdir()
        resolved = {
            "server": {
                "path": rows["server"]["path"],
                "checkout_path": rows["server"]["path"],
                "checkout": "server",
                "repository": rows["server"]["repository"],
                "branch": "main",
                "source": ".",
                "head": rows["server"]["commit"],
                "dirty": False,
            }
        }
        service = {"running": False, "status": "exited", "liveness": "exited"}
        spec = {
            "schema_version": TOPOLOGY_STATUS_SCHEMA_VERSION,
            "name": record["topology"]["name"],
            "profile": record["profile"]["name"],
            "stack": "default",
            "providers": {"server": "server"},
            "dependencies": ["server"],
            "state": "default",
            "build_root": str(self.workspace.paths.builds / "subset"),
            "resolved": resolved,
            "control": None,
            "runtime": {"generation": "a" * 64},
            "services": {"server": service},
            "endpoint": None,
        }
        persisted = {
            **copy.deepcopy(spec),
            "stopped_at": "2026-08-14T00:00:00Z",
            "shutdown": {"control_requested": True, "clean": True},
            "error": None,
            "supervisor": {"running": False, "liveness": "exited"},
            "observation": {
                "process_tree_lease": "released",
                "runtime_bundle_lease": "released",
            },
        }
        atomic_json(topology_root / "spec.json", spec)
        atomic_json(topology_root / "status.json", persisted)
        with mock.patch.object(
            self.workspace, "topology_status", return_value=persisted
        ):
            foreign_spec = copy.deepcopy(spec)
            foreign_status = copy.deepcopy(persisted)
            for value in (foreign_spec, foreign_status):
                value["resolved"]["server"]["path"] = rows["client"]["path"]
                value["resolved"]["server"]["checkout_path"] = rows["client"][
                    "path"
                ]
            atomic_json(topology_root / "spec.json", foreign_spec)
            atomic_json(topology_root / "status.json", foreign_status)
            with mock.patch.object(
                self.workspace, "topology_status", return_value=foreign_status
            ):
                foreign_preview = self.workspace.scope_release(
                    record["name"], apply=False
                )
            self.assertIn(
                "mismatched_topology_records",
                next(
                    item
                    for item in foreign_preview["items"]
                    if item["kind"] == "topology"
                )["reasons"],
            )
            atomic_json(topology_root / "spec.json", spec)
            atomic_json(topology_root / "status.json", persisted)
            preview = self.workspace.scope_release(
                record["name"], apply=False
            )
            self.assertTrue(preview["can_apply"], preview["items"])
            result = self.workspace.scope_release(
                record["name"],
                apply=True,
                plan_sha256=preview["plan_sha256"],
            )
        self.assertTrue(result["released"])
        self.assertFalse(Path(rows["client"]["path"]).exists())
        self.assertFalse(Path(rows["server"]["path"]).exists())

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
                "profile": live["profile"]["name"],
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

        mismatched = self.workspace.scope_create(["client"], name="mismatched")
        Path(mismatched["topology"]["path"]).mkdir()
        with mock.patch.object(
            self.workspace,
            "topology_status",
            return_value={
                "profile": "outside-profile",
                "supervisor": {"running": False, "liveness": "exited"},
                "services": {},
                "observation": {},
            },
        ):
            preview = self.workspace.scope_release("mismatched", apply=False)
        self.assertIn(
            "unexpected_topology_profile",
            next(item for item in preview["items"] if item["kind"] == "topology")[
                "reasons"
            ],
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
                "profile": unreachable["profile"]["name"],
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
                "profile": retained["profile"]["name"],
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

        for name, status in (
            (
                "stale",
                {
                    "schema_version": 3,
                    "profile": "scope-stale",
                    "supervisor": {"running": False, "liveness": "stale"},
                    "services": {},
                    "observation": {
                        "process_tree_lease": "released",
                        "runtime_bundle_lease": "released",
                    },
                },
            ),
            (
                "historical",
                {
                    "schema_version": 1,
                    "profile": "scope-historical",
                    "supervisor": {"running": False, "liveness": "exited"},
                    "services": {},
                    "observation": {
                        "process_tree_lease": "released",
                        "runtime_bundle_lease": "historical",
                    },
                },
            ),
        ):
            record = self.workspace.scope_create(["client"], name=name)
            Path(record["topology"]["path"]).mkdir()
            with mock.patch.object(
                self.workspace, "topology_status", return_value=status
            ):
                preview = self.workspace.scope_release(name, apply=False)
            self.assertIn(
                "unproven_clean_topology_stop",
                next(
                    item for item in preview["items"] if item["kind"] == "topology"
                )["reasons"],
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

    def test_randomized_scope_lifecycle_stress_leaves_no_cross_scope_debris(
        self,
    ) -> None:
        self.make_checkout("client")
        workspace_marker = self.workspace.paths.marker.read_bytes()
        coordination = {
            "provisions": 0,
            "observations": 0,
            "releases": 0,
            "release_conflicts": 0,
        }
        coordination_lock = threading.Lock()
        secret = "scope-stress-secret-must-not-leak"
        with mock.patch.dict(os.environ, {"ATRINIK_SCOPE_STRESS_SENTINEL": secret}):
            for repetition in range(6):
                names = [f"stress-{repetition}-{suffix}" for suffix in ("a", "b")]
                records: dict[str, dict[str, object]] = {}
                provisioned = threading.Barrier(2)
                operation_step = threading.Barrier(2)
                operation_progress = [threading.Condition() for _ in range(4)]
                operation_completions = [0 for _ in range(4)]
                release_start = threading.Barrier(2)
                release_progress = threading.Condition()
                release_transitions = [0]
                release_completed = [0]

                def lifecycle(name: str) -> dict[str, object]:
                    workspace = Workspace(self.wrapper)
                    try:
                        record = workspace.scope_create(["client"], name=name)
                        with coordination_lock:
                            records[name] = record
                            coordination["provisions"] += 1
                        provisioned.wait(timeout=10)
                        other = next(
                            candidate
                            for candidate_name, candidate in records.items()
                            if candidate_name != name
                        )
                        operations = [
                            "show",
                            "path",
                            "preview",
                            "mutation-proof",
                        ]
                        random.Random(
                            404_000 + repetition * 10 + names.index(name)
                        ).shuffle(operations)
                        for step, operation in enumerate(operations):
                            operation_step.wait(timeout=10)
                            if operation == "show":
                                self.assertEqual(workspace.scope_show(name), record)
                            elif operation == "path":
                                self.assertEqual(
                                    workspace.component_path(
                                        "client", record["profile"]["name"]
                                    ),
                                    Path(record["worktrees"][0]["path"]),
                                )
                            elif operation == "preview":
                                for _attempt in range(20):
                                    with operation_progress[step]:
                                        observed = operation_completions[step]
                                    try:
                                        self.assertTrue(
                                            workspace.scope_release(
                                                name, apply=False
                                            )["can_apply"]
                                        )
                                        break
                                    except WorkspaceError as error:
                                        if "active resource leases" not in str(error):
                                            raise
                                        with coordination_lock:
                                            coordination["release_conflicts"] += 1
                                        with operation_progress[step]:
                                            if operation_completions[step] > observed:
                                                continue
                                            if not operation_progress[step].wait_for(
                                                lambda: operation_completions[step]
                                                > observed,
                                                timeout=10,
                                            ):
                                                self.fail(
                                                    "scope preview made no bounded progress"
                                                )
                                else:
                                    self.fail(
                                        "scope preview exhausted bounded retries"
                                    )
                            else:
                                path = Path(record["worktrees"][0]["path"])
                                other_path = Path(other["worktrees"][0]["path"])
                                probe = path / f"scope-{name}.probe"
                                probe.write_text(name, encoding="utf-8")
                                self.assertFalse((other_path / probe.name).exists())
                                self.assertEqual(
                                    probe.read_text(encoding="utf-8"), name
                                )
                                probe.unlink()
                            with coordination_lock:
                                coordination["observations"] += 1
                            with operation_progress[step]:
                                operation_completions[step] += 1
                                operation_progress[step].notify_all()

                        release_start.wait(timeout=10)
                        for _attempt in range(20):
                            try:
                                preview = workspace.scope_release(name, apply=False)
                                workspace.scope_release(
                                    name,
                                    apply=True,
                                    plan_sha256=preview["plan_sha256"],
                                )
                                with coordination_lock:
                                    coordination["releases"] += 1
                                with release_progress:
                                    release_completed[0] += 1
                                    release_transitions[0] += 1
                                    release_progress.notify_all()
                                break
                            except WorkspaceError as error:
                                if "active resource leases" not in str(error):
                                    raise
                                with coordination_lock:
                                    coordination["release_conflicts"] += 1
                                with release_progress:
                                    release_transitions[0] += 1
                                    observed = release_transitions[0]
                                    release_progress.notify_all()
                                    if not release_progress.wait_for(
                                        lambda: release_transitions[0] > observed
                                        or release_completed[0] > 0,
                                        timeout=10,
                                    ):
                                        self.fail(
                                            "disjoint scope release made no bounded progress"
                                        )
                        else:
                            self.fail("disjoint scope release exhausted bounded retries")
                        return record
                    finally:
                        workspace.close()

                completed: list[dict[str, object]] = []
                failures: list[BaseException] = []

                def run_lifecycle(name: str) -> None:
                    try:
                        completed.append(lifecycle(name))
                    except BaseException as error:
                        failures.append(error)

                workers = [
                    threading.Thread(
                        target=run_lifecycle, args=(name,), daemon=True
                    )
                    for name in names
                ]
                for worker in workers:
                    worker.start()
                for worker in workers:
                    worker.join(timeout=30)
                self.assertFalse(
                    any(worker.is_alive() for worker in workers),
                    "scope lifecycle worker exceeded its hard join bound",
                )
                if failures:
                    raise failures[0]
                self.assertEqual(
                    len(
                        {
                            record["worktrees"][0]["path"]
                            for record in completed
                        }
                    ),
                    2,
                )

        self.assertEqual(
            {
                key: coordination[key]
                for key in ("provisions", "observations", "releases")
            },
            {"provisions": 12, "observations": 48, "releases": 12},
        )
        self.assertEqual(self.workspace.paths.marker.read_bytes(), workspace_marker)
        for root in sorted(self.workspace.paths.scopes.iterdir()):
            self.assertTrue((root / "scope.json").is_file())
            creation = json.loads(
                (root / "creation-journal.json").read_text(encoding="utf-8")
            )
            release = json.loads(
                (root / "release-journal.json").read_text(encoding="utf-8")
            )
            self.assertEqual(creation["status"], "complete")
            self.assertEqual(release["status"], "complete")
            self.workspace.scope_show(root.name)
            for evidence in root.glob("*.json"):
                self.assertNotIn(secret, evidence.read_text(encoding="utf-8"))
        self.workspace.close()
        for owners in self.workspace._lease_namespace.rglob("*.owners"):
            locking_module._lease_owner_summary(
                owners.with_name(owners.name.removesuffix(".owners"))
            )
            remaining = list(owners.glob("*.json"))
            self.assertEqual(
                remaining,
                [],
                [json.loads(path.read_text(encoding="utf-8")) for path in remaining],
            )
            self.assertEqual(list((owners / ".pending").iterdir()), [])

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
