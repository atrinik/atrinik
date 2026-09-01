from __future__ import annotations

import fcntl
import json
import os
from pathlib import Path
import shutil
import stat
import subprocess
import tempfile
from types import SimpleNamespace
import unittest
from unittest import mock

from atrinik_workspace import migration as migration_module
from atrinik_workspace.locking import (
    exclusive_layout_lock,
    exclusive_lock,
    inherit_lock_fds,
    layout_writer_intent_path,
)
from atrinik_workspace.migration import RepositoryMigration
from atrinik_workspace.model import Paths, WorkspaceError
from atrinik_workspace.process_tree import control_socket_path, initialize_lease


def command(
    *arguments: str,
    cwd: Path,
    input_value: bytes | None = None,
    environment: dict[str, str] | None = None,
) -> bytes:
    result = subprocess.run(
        list(arguments),
        cwd=cwd,
        check=True,
        input=input_value,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        env=environment,
    )
    return result.stdout


CLASSIC_LOGICAL = (
    ("classic-client", "client", "classic-client"),
    ("classic-server", "server", "classic-server"),
    ("classic-editor", "editor", "none"),
    ("classic-libatrinik", "libatrinik", "classic-library"),
    ("classic-protocol", "protocol", "classic-protocol"),
)
SHARED = (
    "content-1x",
    "playtester",
    "tools",
    "sound",
    "resources",
    "metaserver-worker",
    "devcontainer",
    "github-settings",
)


class RepositoryMigrationTests(unittest.TestCase):
    def test_physical_repository_lock_path_inherits_active_descriptors(self) -> None:
        completed = mock.MagicMock(returncode=0, stdout=".", stderr="")
        with (
            tempfile.TemporaryDirectory() as directory,
            mock.patch(
                "atrinik_workspace.migration.subprocess.run",
                return_value=completed,
            ) as invoke,
        ):
            lock_path = migration_module.physical_repository_lock_path(Path(directory))

        self.assertEqual(
            lock_path,
            Path(directory).resolve()
            / "atrinik-resource-leases"
            / "repository-layout.lock",
        )
        self.assertEqual(invoke.call_args.kwargs["pass_fds"], ())

    def test_git_helpers_inherit_all_exclusive_layout_descriptors(self) -> None:
        completed = mock.MagicMock(returncode=0, stdout=b"", stderr=b"")
        with (
            tempfile.TemporaryDirectory() as directory,
            exclusive_layout_lock(
                Path(directory) / "repository-layout.lock", "repository layout"
            ),
            mock.patch(
                "atrinik_workspace.migration.subprocess.run",
                return_value=completed,
            ) as invoke,
        ):
            RepositoryMigration._git_process(Path("/tmp/repository"), "status")

        self.assertEqual(len(invoke.call_args.kwargs["pass_fds"]), 3)

    def test_git_helpers_inherit_active_layout_descriptor(self) -> None:
        completed = mock.MagicMock(returncode=0, stdout=b"", stderr=b"")
        with (
            tempfile.TemporaryFile(mode="w+") as lease,
            inherit_lock_fds(lease),
            mock.patch(
                "atrinik_workspace.migration.subprocess.run",
                return_value=completed,
            ) as invoke,
        ):
            RepositoryMigration._git_process(Path("/tmp/repository"), "status")
            self.assertEqual(invoke.call_args.kwargs["pass_fds"], (lease.fileno(),))

    def setUp(self) -> None:
        self.temporary = tempfile.TemporaryDirectory()
        self.root = Path(self.temporary.name)
        self.wrapper = self.root / "wrapper"
        self.wrapper.mkdir()
        self.workspace = self.root / "workspace"
        self.environment = mock.patch.dict(
            os.environ, {"ATRINIK_WORKSPACE_DIR": str(self.workspace)}
        )
        self.environment.start()
        self.paths = Paths.discover(self.wrapper)
        self.paths.ensure()

        classic_checkout = SimpleNamespace(
            name="classic",
            repository="atrinik/classic",
            branch="main",
            path="classic",
        )
        components: dict[str, SimpleNamespace] = {}
        for name, source, build in CLASSIC_LOGICAL:
            components[name] = SimpleNamespace(
                name=name,
                repository="atrinik/classic",
                branch="main",
                checkout="classic",
                checkout_name="classic",
                source=source,
                build=build,
            )
        for name in SHARED:
            components[name] = SimpleNamespace(
                name=name,
                repository=f"atrinik/{'content' if name == 'content-1x' else name}",
                branch="1.x" if name == "content-1x" else "main",
                checkout=name,
                checkout_name=name,
                source=".",
                build="none",
            )
        self.manifest = SimpleNamespace(
            by_name=components,
            by_checkout={"classic": classic_checkout},
            stacks={
                "classic": SimpleNamespace(
                    name="classic",
                    components=tuple(components.values()),
                )
            },
        )
        self.anchor_patch = mock.patch.dict(
            migration_module.CLASSIC_HISTORY_ANCHORS, {}, clear=True
        )
        self.anchor_patch.start()

    def tearDown(self) -> None:
        self.anchor_patch.stop()
        self.environment.stop()
        self.temporary.cleanup()

    def migration(self) -> RepositoryMigration:
        return RepositoryMigration(
            self.wrapper,
            self.paths,
            self.manifest,
            self.paths.workspace / "physical-repository-layout.lock",
            lambda _transitions=None: None,
        )

    def test_classic_profile_fallback_includes_playtester(self) -> None:
        migration = self.migration()
        migration.manifest = SimpleNamespace()

        self.assertIn("playtester", migration._classic_profile_component_names())

    def make_repository(
        self,
        path_name: str,
        canonical: str,
        *,
        repository: str | None = None,
        classic: bool = True,
        text: str | None = None,
    ) -> Path:
        path = self.wrapper / path_name
        path.mkdir()
        command("git", "init", "-b", "main", cwd=path)
        command("git", "config", "user.name", "Migration Tests", cwd=path)
        command(
            "git",
            "config",
            "user.email",
            "migration@example.invalid",
            cwd=path,
        )
        (path / "tracked.txt").write_text(
            text if text is not None else f"classic {canonical}\n", encoding="utf-8"
        )
        command("git", "add", "tracked.txt", cwd=path)
        command("git", "commit", "-m", "feat: seed source", cwd=path)
        if classic:
            migration_module.CLASSIC_HISTORY_ANCHORS[canonical] = command(
                "git", "rev-parse", "HEAD", cwd=path
            ).decode().strip()
        command(
            "git",
            "remote",
            "add",
            "origin",
            "https://github.com/"
            + (repository or f"atrinik/legacy-{canonical}")
            + ".git",
            cwd=path,
        )
        return path

    def make_classic(self, sources: dict[str, Path] | None = None) -> Path:
        sources = sources or {}
        path = self.wrapper / "classic"
        path.mkdir()
        command("git", "init", "-b", "main", cwd=path)
        command("git", "config", "user.name", "Migration Tests", cwd=path)
        command(
            "git",
            "config",
            "user.email",
            "migration@example.invalid",
            cwd=path,
        )
        for _, prefix, _ in CLASSIC_LOGICAL:
            directory = path / prefix
            directory.mkdir()
            source = sources.get(prefix)
            if source is None:
                (directory / "tracked.txt").write_text(
                    f"classic {prefix}\n", encoding="utf-8"
                )
            else:
                shutil.copy2(source / "tracked.txt", directory / "tracked.txt")
        command("git", "add", ".", cwd=path)
        command("git", "commit", "-m", "feat: assemble classic", cwd=path)
        command(
            "git",
            "remote",
            "add",
            "origin",
            "https://github.com/atrinik/classic.git",
            cwd=path,
        )
        return path

    def write_profile(
        self,
        name: str,
        components: dict[str, dict[str, str]],
        *,
        schema: int = 1,
    ) -> Path:
        value: dict[str, object] = {
            "schema_version": schema,
            "name": name,
            "components": components,
        }
        if schema == 2:
            value["stack"] = "classic"
        path = self.paths.profiles / f"{name}.json"
        path.write_text(
            json.dumps(value, indent=2, sort_keys=True) + "\n", encoding="utf-8"
        )
        return path

    @staticmethod
    def status_bytes(path: Path) -> bytes:
        return command(
            "git",
            "status",
            "--porcelain=v1",
            "-z",
            "--untracked-files=all",
            "--no-renames",
            cwd=path,
        )

    @staticmethod
    def prefixed_status(value: bytes, prefix: str) -> bytes:
        result = bytearray()
        for row in value.split(b"\0"):
            if row:
                result.extend(row[:3] + prefix.encode() + b"/" + row[3:] + b"\0")
        return bytes(result)

    def assert_single_classic_selector(
        self,
        profile: dict[str, object],
        expected: dict[str, str],
    ) -> None:
        components = profile["components"]
        assert isinstance(components, dict)
        selectors = {
            tuple(sorted(selector.items()))
            for name, selector in components.items()
            if name.startswith("classic-")
        }
        self.assertEqual(selectors, {tuple(sorted(expected.items()))})

    def test_dry_run_is_deterministic_and_writes_nothing(self) -> None:
        source = self.make_repository("client", "client")
        self.make_classic({"client": source})
        profile = self.write_profile(
            "review", {"client": {"kind": "primary", "value": ""}}
        )
        before_profile = profile.read_bytes()
        before_index = (source / ".git" / "index").read_bytes()

        first = self.migration().execute("dry-run")
        second = self.migration().execute("dry-run")

        self.assertEqual(first, second)
        self.assertEqual(first["status"], "ready")
        self.assertEqual(first["refusals"], [])
        self.assertEqual(first["sources"][0]["source"], str(source))
        self.assertEqual(profile.read_bytes(), before_profile)
        self.assertEqual((source / ".git" / "index").read_bytes(), before_index)
        self.assertTrue(source.is_dir())
        self.assertFalse((self.workspace / "archive").exists())
        self.assertFalse((self.workspace / "migrations").exists())

    def test_unrecognized_schema_v2_profile_fails_closed(self) -> None:
        source = self.make_repository("client", "client")
        self.make_classic({"client": source})
        profile = self.write_profile(
            "replacement",
            {"client": {"kind": "primary", "value": ""}},
            schema=2,
        )
        value = json.loads(profile.read_text(encoding="utf-8"))
        value["stack"] = "default"
        profile.write_text(
            json.dumps(value, indent=2, sort_keys=True) + "\n",
            encoding="utf-8",
        )
        before = profile.read_bytes()

        result = self.migration().execute("apply")

        self.assertEqual(result["status"], "refused")
        self.assertIn("invalid_profile", {row["code"] for row in result["refusals"]})
        self.assertTrue(source.is_dir())
        self.assertEqual(profile.read_bytes(), before)

    def test_apply_archives_clean_source_and_rewrites_profile_to_schema_v5(self) -> None:
        source = self.make_repository("client", "client")
        classic = self.make_classic({"client": source})
        head = command("git", "rev-parse", "HEAD", cwd=source).decode().strip()
        profile = self.write_profile(
            "review",
            {
                "client": {"kind": "primary", "value": ""},
                "content": {"kind": "primary", "value": ""},
            },
        )
        profile.chmod(0o600)

        result = self.migration().execute("apply")

        archive = (
            self.workspace
            / "archive"
            / "classic-migration"
            / "repositories"
            / "client"
        )
        self.assertEqual(result["status"], "applied")
        self.assertFalse(source.exists())
        self.assertTrue(archive.is_dir())
        self.assertEqual(command("git", "rev-parse", "HEAD", cwd=archive).decode().strip(), head)
        self.assertEqual(self.status_bytes(archive), b"")
        self.assertEqual(self.status_bytes(classic), b"")
        value = json.loads(profile.read_text(encoding="utf-8"))
        self.assertEqual(value["schema_version"], 5)
        self.assertEqual(value["sound_mode"], "source")
        self.assertIsNone(value["sound_release"])
        self.assertEqual(value["stack"], "classic")
        self.assertEqual(stat.S_IMODE(profile.stat().st_mode), 0o600)
        self.assertEqual(
            value["components"]["classic-client"],
            {"kind": "primary", "value": ""},
        )
        self.assert_single_classic_selector(
            value,
            {"kind": "primary", "value": ""},
        )
        self.assertEqual(
            value["components"]["content-1x"],
            {"kind": "primary", "value": ""},
        )
        self.assertNotIn("client", value["components"])
        audit = self.migration().execute("audit")
        self.assertEqual(audit["status"], "complete", audit)
        self.assertEqual(self.migration().execute("apply")["status"], "already-applied")

    def test_record_fsync_uncertainty_does_not_rollback_committed_layout(self) -> None:
        source = self.make_repository("client", "client")
        self.make_classic({"client": source})
        profile = self.write_profile(
            "review",
            {
                "client": {"kind": "primary", "value": ""},
                "content": {"kind": "primary", "value": ""},
            },
        )
        migration = self.migration()
        real_publish = migration._durable_atomic_json

        def uncertain(path: Path, value: object) -> None:
            real_publish(path, value)
            if path == migration.record_path:
                raise migration_module.AtomicJsonCommitUncertain(
                    "simulated record durability uncertainty"
                )

        with (
            mock.patch.object(migration, "_durable_atomic_json", side_effect=uncertain),
            self.assertRaisesRegex(WorkspaceError, "migration committed"),
        ):
            migration.execute("apply")

        self.assertTrue(migration.record_path.is_file())
        self.assertFalse(source.exists())
        self.assertNotEqual(json.loads(profile.read_text())["schema_version"], 1)

    def test_already_applied_retry_rejects_symlinked_pending_journal(self) -> None:
        source = self.make_repository("client", "client")
        self.make_classic({"client": source})
        self.write_profile(
            "review",
            {
                "client": {"kind": "primary", "value": ""},
                "content": {"kind": "primary", "value": ""},
            },
        )
        migration = self.migration()
        real_unlink = migration_module.unlink_validated_json

        def retain_pending(path: Path, validator: object) -> None:
            if path == migration.pending_path:
                raise WorkspaceError("retain journal")
            real_unlink(path, validator)

        with mock.patch.object(
            migration_module,
            "unlink_validated_json",
            side_effect=retain_pending,
        ):
            self.assertEqual(migration.execute("apply")["status"], "applied")
        external = self.root / "external-pending.json"
        migration.pending_path.rename(external)
        migration.pending_path.symlink_to(external)

        with self.assertRaisesRegex(WorkspaceError, "cannot consume"):
            migration.execute("apply")

        self.assertTrue(external.is_file())

    def test_schema_v3_replacement_profile_is_valid_and_left_unchanged(self) -> None:
        profile = self.paths.profiles / "replacement.json"
        value = {
            "schema_version": 3,
            "name": "replacement",
            "stack": "default",
            "components": {
                "client": {"kind": "primary", "value": ""},
                "sound": {"kind": "primary", "value": ""},
            },
        }
        profile.write_text(
            json.dumps(value, indent=2, sort_keys=True) + "\n", encoding="utf-8"
        )

        rewritten, composite = self.migration()._rewrite_profile(
            profile, value, {}, None
        )

        self.assertIsNone(rewritten)
        self.assertIsNone(composite)
        self.assertEqual(json.loads(profile.read_text(encoding="utf-8")), value)

    def test_schema_v3_classic_profile_upgrades_to_source_sound(self) -> None:
        migration = self.migration()
        profile = self.paths.profiles / "classic.json"
        selector = {"kind": "primary", "value": ""}
        value = {
            "schema_version": 3,
            "name": "classic",
            "stack": "classic",
            "components": {
                name: dict(selector)
                for name in migration._classic_profile_component_names()
            },
        }

        rewritten, composite = migration._rewrite_profile(profile, value, {}, None)

        self.assertIsNone(composite)
        self.assertEqual(
            rewritten,
            {
                **value,
                "schema_version": 5,
                "sound_mode": "source",
                "sound_release": None,
            },
        )

    def test_current_and_legacy_profile_shapes_fail_closed(self) -> None:
        migration = self.migration()
        profile = self.paths.profiles / "classic.json"
        components = {
            name: {"kind": "primary", "value": ""}
            for name in migration._classic_profile_component_names()
        }
        current = {
            "schema_version": 5,
            "name": "classic",
            "stack": "classic",
            "sound_mode": "source",
            "sound_release": None,
            "components": components,
        }
        current_mutations = {
            "unexpected field": {**current, "unexpected": True},
            "wrong schema": {**current, "schema_version": 3},
            "invalid sound mode": {**current, "sound_mode": "released"},
            "wrong identity": {**current, "name": "other"},
            "non-object components": {**current, "components": []},
            "incomplete closure": {
                **current,
                "components": dict(list(components.items())[1:]),
            },
            "split classic checkout": {
                **current,
                "components": {
                    **components,
                    "classic-client": {"kind": "worktree", "value": "split"},
                },
            },
        }
        for label, malformed in current_mutations.items():
            with self.subTest(schema="current", case=label):
                with self.assertRaises(WorkspaceError):
                    migration._validate_profile_shape(profile, malformed)

        legacy = {
            "schema_version": 3,
            "name": "classic",
            "stack": "classic",
            "components": components,
        }
        legacy_mutations = {
            "unexpected field": {**legacy, "unexpected": True},
            "invalid identity": {**legacy, "stack": ""},
            "invalid component": {
                **legacy,
                "components": {"": {"kind": "primary", "value": ""}},
            },
        }
        for label, malformed in legacy_mutations.items():
            with self.subTest(schema="legacy", case=label):
                with self.assertRaises(WorkspaceError):
                    migration._validate_legacy_profile_shape(profile, malformed)

    def test_replacement_profile_cannot_enable_local_playtest_sound(self) -> None:
        profile = self.paths.profiles / "replacement.json"
        value = {
            "schema_version": 5,
            "name": "replacement",
            "stack": "default",
            "sound_mode": "local-playtest",
            "sound_release": None,
            "components": {},
        }

        with self.assertRaisesRegex(WorkspaceError, "Classic-derived"):
            self.migration()._rewrite_profile(profile, value, {}, None)

    def test_schema_v4_replacement_profiles_are_validated_and_left_inert(self) -> None:
        profile = self.paths.profiles / "replacement.json"
        value = {
            "schema_version": 4,
            "name": "replacement",
            "stack": "default",
            "sound_mode": "source",
            "components": {},
        }
        rewritten, composite = self.migration()._rewrite_profile(
            profile, value, {}, None
        )
        self.assertIsNone(rewritten)
        self.assertIsNone(composite)
        for malformed in (
            {**value, "name": "other"},
            {**value, "sound_mode": "local-playtest"},
            {**value, "unexpected": True},
            {**value, "components": []},
        ):
            with self.subTest(malformed=malformed):
                with self.assertRaisesRegex(
                    WorkspaceError, "schema-v4 replacement profile"
                ):
                    self.migration()._rewrite_profile(profile, malformed, {}, None)

    def test_audit_accepts_manifest_owned_replacement_at_reused_source_path(
        self,
    ) -> None:
        source = self.make_repository("client", "client")
        self.make_classic({"client": source})

        result = self.migration().execute("apply")
        self.assertEqual(result["status"], "applied")
        replacement = self.make_repository(
            "client",
            "client",
            repository="atrinik/client",
            classic=False,
            text="replacement client\n",
        )
        self.manifest.by_checkout["client"] = SimpleNamespace(
            name="client",
            repository="atrinik/client",
            branch="main",
            path="client",
            generation="replacement",
        )
        (replacement / "local-change.txt").write_text(
            "preserve me\n", encoding="utf-8"
        )
        status_before = self.status_bytes(replacement)

        audit = self.migration().execute("audit")

        self.assertEqual(audit["status"], "complete", audit)
        self.assertEqual(self.status_bytes(replacement), status_before)

    def test_audit_rejects_wrong_repository_at_reused_source_path(self) -> None:
        source = self.make_repository("client", "client")
        self.make_classic({"client": source})
        self.assertEqual(self.migration().execute("apply")["status"], "applied")
        self.make_repository(
            "client",
            "client",
            repository="someone-else/client",
            classic=False,
            text="unrelated client\n",
        )
        self.manifest.by_checkout["client"] = SimpleNamespace(
            name="client",
            repository="atrinik/client",
            branch="main",
            path="client",
            generation="replacement",
        )

        audit = self.migration().execute("audit")

        self.assertEqual(audit["status"], "incomplete")
        self.assertIn(
            "source_archive_audit_failed",
            {row["code"] for row in audit["refusals"]},
        )

    def test_audit_rejects_later_matching_fetch_url(self) -> None:
        source = self.make_repository("client", "client")
        self.make_classic({"client": source})
        self.assertEqual(self.migration().execute("apply")["status"], "applied")
        replacement = self.make_repository(
            "client",
            "client",
            repository="someone-else/client",
            classic=False,
            text="unrelated client\n",
        )
        command(
            "git",
            "remote",
            "set-url",
            "--add",
            "origin",
            "https://github.com/atrinik/client.git",
            cwd=replacement,
        )
        self.manifest.by_checkout["client"] = SimpleNamespace(
            name="client",
            repository="atrinik/client",
            branch="main",
            path="client",
            generation="replacement",
        )

        audit = self.migration().execute("audit")

        self.assertEqual(audit["status"], "incomplete")
        self.assertIn(
            "source_archive_audit_failed",
            {row["code"] for row in audit["refusals"]},
        )

    def test_audit_rejects_classic_history_disguised_as_replacement(self) -> None:
        source = self.make_repository("client", "client")
        self.make_classic({"client": source})
        result = self.migration().execute("apply")
        self.assertEqual(result["status"], "applied")
        archive = Path(result["sources"][0]["archive"])
        command("git", "clone", str(archive), str(source), cwd=self.wrapper)
        command(
            "git",
            "remote",
            "set-url",
            "origin",
            "https://github.com/atrinik/client.git",
            cwd=source,
        )
        self.manifest.by_checkout["client"] = SimpleNamespace(
            name="client",
            repository="atrinik/client",
            branch="main",
            path="client",
            generation="replacement",
        )

        audit = self.migration().execute("audit")

        self.assertEqual(audit["status"], "incomplete")
        self.assertIn(
            "source_archive_audit_failed",
            {row["code"] for row in audit["refusals"]},
        )

    def test_audit_rejects_replacement_path_nested_in_another_checkout(self) -> None:
        source = self.make_repository("client", "client")
        self.make_classic({"client": source})
        self.assertEqual(self.migration().execute("apply")["status"], "applied")
        source.mkdir()
        command("git", "init", "-b", "main", cwd=self.wrapper)
        command("git", "config", "user.name", "Migration Tests", cwd=self.wrapper)
        command(
            "git",
            "config",
            "user.email",
            "migration@example.invalid",
            cwd=self.wrapper,
        )
        (source / "tracked.txt").write_text("nested replacement\n", encoding="utf-8")
        command("git", "add", "client/tracked.txt", cwd=self.wrapper)
        command("git", "commit", "-m", "feat: nested replacement", cwd=self.wrapper)
        command(
            "git",
            "remote",
            "add",
            "origin",
            "https://github.com/atrinik/client.git",
            cwd=self.wrapper,
        )
        self.manifest.by_checkout["client"] = SimpleNamespace(
            name="client",
            repository="atrinik/client",
            branch="main",
            path="client",
            generation="replacement",
        )

        audit = self.migration().execute("audit")

        self.assertEqual(audit["status"], "incomplete")
        self.assertIn(
            "source_archive_audit_failed",
            {row["code"] for row in audit["refusals"]},
        )

    def test_audit_rejects_symlink_at_reused_source_path(self) -> None:
        source = self.make_repository("client", "client")
        self.make_classic({"client": source})
        result = self.migration().execute("apply")
        self.assertEqual(result["status"], "applied")
        archive = Path(result["sources"][0]["archive"])
        source.symlink_to(archive, target_is_directory=True)
        self.manifest.by_checkout["client"] = SimpleNamespace(
            name="client",
            repository="atrinik/client",
            branch="main",
            path="client",
            generation="replacement",
        )

        audit = self.migration().execute("audit")

        self.assertEqual(audit["status"], "incomplete")
        self.assertIn(
            "source_archive_audit_failed",
            {row["code"] for row in audit["refusals"]},
        )

    def test_audit_rejects_reused_source_path_sharing_archive_git_dir(self) -> None:
        source = self.make_repository("client", "client")
        self.make_classic({"client": source})
        result = self.migration().execute("apply")
        self.assertEqual(result["status"], "applied")
        archive = Path(result["sources"][0]["archive"])
        command(
            "git",
            "worktree",
            "add",
            "-b",
            "replacement-mask",
            str(source),
            cwd=archive,
        )
        self.manifest.by_checkout["client"] = SimpleNamespace(
            name="client",
            repository="atrinik/legacy-client",
            branch="main",
            path="client",
            generation="replacement",
        )

        audit = self.migration().execute("audit")

        self.assertEqual(audit["status"], "incomplete")
        self.assertIn(
            "source_archive_audit_failed",
            {row["code"] for row in audit["refusals"]},
        )

    def test_content_1x_worktree_becomes_protected_migration_selector(self) -> None:
        source = self.make_repository("client", "client")
        self.make_classic({"client": source})
        content = self.make_repository(
            "content",
            "content",
            repository="atrinik/content",
            classic=False,
        )
        command("git", "branch", "1.x", cwd=content)
        selected = self.paths.worktrees / "content" / "classic-maps"
        selected.parent.mkdir(parents=True)
        command(
            "git",
            "worktree",
            "add",
            "-b",
            "feat/classic-maps",
            str(selected),
            cwd=content,
        )
        profile = self.write_profile(
            "maps",
            {
                "client": {"kind": "primary", "value": ""},
                "content": {"kind": "worktree", "value": "classic-maps"},
            },
        )

        result = self.migration().execute("apply")

        self.assertEqual(result["status"], "applied")
        value = json.loads(profile.read_text(encoding="utf-8"))
        self.assertEqual(
            value["components"]["content-1x"],
            {"kind": "migrated-worktree", "value": str(selected.resolve())},
        )
        self.assertTrue(content.is_dir())
        self.assertTrue(selected.is_dir())

    def test_already_renamed_source_coexists_with_fresh_canonical_checkout(self) -> None:
        replacement = self.make_repository(
            "client",
            "client",
            repository="atrinik/client",
            classic=False,
            text="replacement\n",
        )
        source = self.make_repository("legacy-client", "client")
        self.make_classic({"client": source})

        result = self.migration().execute("apply")

        archive = (
            self.workspace
            / "archive"
            / "classic-migration"
            / "repositories"
            / "legacy-client"
        )
        self.assertEqual(result["status"], "applied")
        self.assertTrue(replacement.is_dir())
        self.assertFalse(source.exists())
        self.assertTrue(archive.is_dir())
        row = next(row for row in result["sources"] if row["component"] == "classic-client")
        self.assertEqual(row["layout"], "legacy-client")

    def test_dirty_primary_preserves_staged_unstaged_and_untracked_state(self) -> None:
        source = self.make_repository("client", "client")
        classic = self.make_classic({"client": source})
        (source / "staged.txt").write_text("staged\n", encoding="utf-8")
        command("git", "add", "staged.txt", cwd=source)
        (source / "tracked.txt").write_text("unstaged\n", encoding="utf-8")
        (source / "untracked.bin").write_bytes(b"untracked\x00bytes")
        before = self.status_bytes(source)
        profile = self.write_profile(
            "dirty", {"client": {"kind": "primary", "value": ""}}
        )

        result = self.migration().execute("apply")

        row = next(
            row
            for row in result["worktree_migrations"]
            if row["component"] == "classic-client" and row["primary"]
        )
        migrated = Path(row["destination"])
        archive = Path(
            next(
                source_row["archive"]
                for source_row in result["sources"]
                if source_row["component"] == "classic-client"
            )
        )
        self.assertTrue(migrated.is_dir())
        self.assertEqual(self.status_bytes(migrated), self.prefixed_status(before, "client"))
        self.assertEqual(self.status_bytes(archive), before)
        self.assertEqual((migrated / "client" / "tracked.txt").read_text(), "unstaged\n")
        self.assertEqual(
            (migrated / "client" / "untracked.bin").read_bytes(),
            b"untracked\x00bytes",
        )
        self.assertTrue((migrated / "server" / "tracked.txt").is_file())
        source_head = command("git", "rev-parse", "HEAD", cwd=archive).decode().strip()
        migrated_parents = command(
            "git", "show", "-s", "--format=%P", "HEAD", cwd=migrated
        ).decode().split()
        self.assertIn(source_head, migrated_parents)
        value = json.loads(profile.read_text(encoding="utf-8"))
        self.assertEqual(
            value["components"]["classic-client"],
            {"kind": "worktree", "value": row["label"]},
        )
        self.assert_single_classic_selector(
            value,
            {"kind": "worktree", "value": row["label"]},
        )
        self.assertEqual(self.status_bytes(classic), b"")

    def test_dirty_linked_worktree_is_copied_to_full_classic_worktree(self) -> None:
        source = self.make_repository("client", "client")
        self.make_classic({"client": source})
        linked = self.paths.worktrees / "client" / "review"
        linked.parent.mkdir(parents=True)
        command(
            "git", "worktree", "add", "-b", "feat/review", str(linked), cwd=source
        )
        (linked / "tracked.txt").write_text("feature committed\n", encoding="utf-8")
        command("git", "add", "tracked.txt", cwd=linked)
        command("git", "commit", "-m", "feat: local feature", cwd=linked)
        (linked / "staged.txt").write_text("staged\n", encoding="utf-8")
        command("git", "add", "staged.txt", cwd=linked)
        (linked / "tracked.txt").write_text("feature unstaged\n", encoding="utf-8")
        (linked / "untracked.txt").write_text("untracked\n", encoding="utf-8")
        before = self.status_bytes(linked)
        head = command("git", "rev-parse", "HEAD", cwd=linked)
        profile = self.write_profile(
            "linked", {"client": {"kind": "worktree", "value": "review"}}
        )

        result = self.migration().execute("apply")

        row = next(
            row
            for row in result["worktree_migrations"]
            if row["component"] == "classic-client" and not row["primary"]
        )
        migrated = Path(row["destination"])
        self.assertEqual(self.status_bytes(linked), before)
        self.assertEqual(command("git", "rev-parse", "HEAD", cwd=linked), head)
        common = command("git", "rev-parse", "--git-common-dir", cwd=linked).decode().strip()
        self.assertIn("archive/classic-migration/repositories/client", common)
        self.assertEqual(self.status_bytes(migrated), self.prefixed_status(before, "client"))
        self.assertTrue((migrated / "server" / "tracked.txt").is_file())
        value = json.loads(profile.read_text(encoding="utf-8"))
        self.assertEqual(
            value["components"]["classic-client"],
            {"kind": "worktree", "value": row["label"]},
        )
        self.assert_single_classic_selector(
            value,
            {"kind": "worktree", "value": row["label"]},
        )

    def test_detached_locked_worktree_remains_attached_and_locked(self) -> None:
        source = self.make_repository("client", "client")
        self.make_classic({"client": source})
        linked = self.paths.worktrees / "client" / "detached"
        linked.parent.mkdir(parents=True)
        command("git", "worktree", "add", "--detach", str(linked), cwd=source)
        command(
            "git",
            "worktree",
            "lock",
            "--reason",
            "preserve review",
            str(linked),
            cwd=source,
        )
        (linked / "tracked.txt").write_text("detached work\n", encoding="utf-8")
        before = self.status_bytes(linked)
        profile = self.write_profile(
            "detached",
            {"client": {"kind": "worktree", "value": "detached"}},
        )

        result = self.migration().execute("apply")

        row = next(
            row
            for row in result["worktree_migrations"]
            if row["component"] == "classic-client" and not row["primary"]
        )
        self.assertIsNone(row["branch"])
        self.assertEqual(row["locked"], "preserve review")
        self.assertEqual(self.status_bytes(linked), before)
        detached = subprocess.run(
            ["git", "symbolic-ref", "--quiet", "--short", "HEAD"],
            cwd=linked,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            check=False,
        )
        self.assertNotEqual(detached.returncode, 0)
        listing = command(
            "git", "worktree", "list", "--porcelain", cwd=linked
        ).decode()
        self.assertIn("locked preserve review", listing)
        value = json.loads(profile.read_text(encoding="utf-8"))
        self.assert_single_classic_selector(
            value,
            {"kind": "worktree", "value": row["label"]},
        )
        audit = self.migration().execute("audit")
        self.assertEqual(audit["status"], "complete", audit)

    def test_same_status_concurrent_change_invalidates_apply_snapshot(self) -> None:
        source = self.make_repository("client", "client")
        self.make_classic({"client": source})
        (source / "staged.txt").write_text("first\n", encoding="utf-8")
        command("git", "add", "staged.txt", cwd=source)
        self.write_profile(
            "concurrent", {"client": {"kind": "primary", "value": ""}}
        )
        original = RepositoryMigration._create_classic_worktree
        changed = False

        def mutate_then_create(
            migration: RepositoryMigration,
            classic: object,
            worktree: object,
        ) -> None:
            nonlocal changed
            if not changed:
                changed = True
                (source / "staged.txt").write_text("second\n", encoding="utf-8")
                command("git", "add", "staged.txt", cwd=source)
            original(migration, classic, worktree)

        with mock.patch.object(
            RepositoryMigration,
            "_create_classic_worktree",
            new=mutate_then_create,
        ):
            with self.assertRaisesRegex(WorkspaceError, "worktree changed"):
                self.migration().execute("apply")

        self.assertTrue(source.is_dir())
        self.assertEqual((source / "staged.txt").read_text(), "second\n")
        self.assertFalse(self.migration().pending_path.exists())
        self.assertFalse(self.migration().record_path.exists())

    def test_profile_with_multiple_component_worktrees_uses_one_composite(self) -> None:
        client = self.make_repository("client", "client")
        server = self.make_repository("server", "server")
        self.make_classic({"client": client, "server": server})
        selected: dict[str, tuple[Path, bytes]] = {}
        for canonical, source in (("client", client), ("server", server)):
            linked = self.paths.worktrees / canonical / "review"
            linked.parent.mkdir(parents=True)
            command(
                "git",
                "worktree",
                "add",
                "-b",
                f"feat/{canonical}-review",
                str(linked),
                cwd=source,
            )
            (linked / "tracked.txt").write_text(
                f"committed {canonical}\n",
                encoding="utf-8",
            )
            command("git", "add", "tracked.txt", cwd=linked)
            command("git", "commit", "-m", f"feat: change {canonical}", cwd=linked)
            (linked / "staged.txt").write_text(
                f"staged {canonical}\n",
                encoding="utf-8",
            )
            command("git", "add", "staged.txt", cwd=linked)
            (linked / "untracked.txt").write_text(
                f"untracked {canonical}\n",
                encoding="utf-8",
            )
            selected[canonical] = (linked, self.status_bytes(linked))
        profile = self.write_profile(
            "combined",
            {
                "client": {"kind": "worktree", "value": "review"},
                "server": {"kind": "worktree", "value": "review"},
            },
        )

        planned = self.migration().execute("dry-run")
        self.assertEqual(len(planned["composite_worktrees"]), 1)
        result = self.migration().execute("apply")

        composite_row = result["composite_worktrees"][0]
        composite = Path(composite_row["destination"])
        expected_rows = []
        for canonical, (linked, before) in selected.items():
            self.assertEqual(self.status_bytes(linked), before)
            expected_rows.extend(
                row
                for row in self.prefixed_status(before, canonical).split(b"\0")
                if row
            )
        expected_rows.sort(key=lambda row: (row[:2] == b"??", row[3:]))
        expected_status = b"".join(row + b"\0" for row in expected_rows)
        self.assertEqual(self.status_bytes(composite), expected_status)
        self.assertEqual(
            (composite / "client" / "tracked.txt").read_text(),
            "committed client\n",
        )
        self.assertEqual(
            (composite / "server" / "tracked.txt").read_text(),
            "committed server\n",
        )
        value = json.loads(profile.read_text(encoding="utf-8"))
        self.assert_single_classic_selector(
            value,
            {"kind": "worktree", "value": composite_row["label"]},
        )
        audit = self.migration().execute("audit")
        self.assertEqual(audit["status"], "complete", audit)

    def test_commit_map_target_is_used_as_bridge_parent(self) -> None:
        source = self.make_repository("client", "client")
        classic = self.make_classic({"client": source})
        linked = self.paths.worktrees / "client" / "mapped"
        linked.parent.mkdir(parents=True)
        command("git", "worktree", "add", "-b", "feat/mapped", str(linked), cwd=source)
        (linked / "tracked.txt").write_text("mapped feature\n", encoding="utf-8")
        command("git", "add", "tracked.txt", cwd=linked)
        command("git", "commit", "-m", "feat: mapped feature", cwd=linked)
        old_head = command("git", "rev-parse", "HEAD", cwd=linked).decode().strip()
        mapped = self.make_rewritten_commit(classic, linked, "client")
        map_path = classic / "docs" / "history"
        map_path.mkdir(parents=True)
        (map_path / "client-commit-map.txt").write_text(
            f"old                                      new\n{old_head} {mapped}\n",
            encoding="ascii",
        )
        command("git", "add", "docs/history/client-commit-map.txt", cwd=classic)
        command("git", "commit", "-m", "docs: record import map", cwd=classic)

        result = self.migration().execute("apply")

        row = next(
            row
            for row in result["worktree_migrations"]
            if row["component"] == "classic-client" and not row["primary"]
        )
        self.assertEqual(row["mapped_parent"], mapped)
        migrated = Path(row["destination"])
        parents = command("git", "show", "-s", "--format=%P", "HEAD", cwd=migrated).decode().split()
        self.assertIn(mapped, parents)

    def test_absent_branch_only_map_target_bridges_from_source_head(self) -> None:
        source = self.make_repository("client", "client")
        classic = self.make_classic({"client": source})
        linked = self.paths.worktrees / "client" / "branch-only"
        linked.parent.mkdir(parents=True)
        command(
            "git",
            "worktree",
            "add",
            "-b",
            "feat/branch-only",
            str(linked),
            cwd=source,
        )
        (linked / "tracked.txt").write_text(
            "branch-only feature\n", encoding="utf-8"
        )
        command("git", "add", "tracked.txt", cwd=linked)
        command("git", "commit", "-m", "feat: branch-only feature", cwd=linked)
        old_head = command("git", "rev-parse", "HEAD", cwd=linked).decode().strip()
        unavailable = "f" * 40
        history = classic / "docs" / "history"
        history.mkdir(parents=True)
        (history / "client-commit-map.txt").write_text(
            f"old                                      new\n{old_head} {unavailable}\n",
            encoding="ascii",
        )
        command("git", "add", "docs/history/client-commit-map.txt", cwd=classic)
        command("git", "commit", "-m", "docs: record branch-only map", cwd=classic)

        result = self.migration().execute("apply")

        row = next(
            row
            for row in result["worktree_migrations"]
            if row["component"] == "classic-client" and not row["primary"]
        )
        self.assertIsNone(row["mapped_parent"])
        migrated = Path(row["destination"])
        parents = command(
            "git", "show", "-s", "--format=%P", "HEAD", cwd=migrated
        ).decode().split()
        self.assertIn(old_head, parents)
        self.assertEqual(self.migration().execute("audit")["status"], "complete")

    def test_malformed_exact_commit_map_fails_closed_without_writes(self) -> None:
        source = self.make_repository("client", "client")
        classic = self.make_classic({"client": source})
        linked = self.paths.worktrees / "client" / "mapped"
        linked.parent.mkdir(parents=True)
        command("git", "worktree", "add", "-b", "feat/mapped", str(linked), cwd=source)
        history = classic / "docs" / "history"
        history.mkdir(parents=True)
        (history / "client-commit-map.txt").write_text(
            "# old\\tnew\nnot-a-tab-separated-map-row\n",
            encoding="ascii",
        )
        command("git", "add", "docs/history/client-commit-map.txt", cwd=classic)
        command("git", "commit", "-m", "docs: add invalid map", cwd=classic)

        result = self.migration().execute("apply")

        self.assertEqual(result["status"], "refused")
        self.assertIn(
            "invalid_source_worktree",
            {row["code"] for row in result["refusals"]},
        )
        self.assertTrue(source.is_dir())
        self.assertTrue(linked.is_dir())
        self.assertFalse((self.workspace / "migrations").exists())

    def make_rewritten_commit(self, classic: Path, source: Path, prefix: str) -> str:
        head = command("git", "rev-parse", "HEAD", cwd=source).decode().strip()
        command(
            "git",
            "fetch",
            "--no-tags",
            str(source),
            f"+{head}:refs/heads/test-source",
            cwd=classic,
        )
        descriptor, index_name = tempfile.mkstemp(dir=self.root)
        os.close(descriptor)
        index = Path(index_name)
        index.unlink()
        environment = os.environ.copy()
        environment["GIT_INDEX_FILE"] = str(index)
        try:
            command("git", "read-tree", "--empty", cwd=classic, environment=environment)
            command(
                "git",
                "read-tree",
                f"--prefix={prefix}/",
                f"{head}^{{tree}}",
                cwd=classic,
                environment=environment,
            )
            tree = command(
                "git", "write-tree", cwd=classic, environment=environment
            ).decode().strip()
        finally:
            index.unlink(missing_ok=True)
        return command(
            "git",
            "commit-tree",
            tree,
            cwd=classic,
            input_value=b"chore: rewritten source\n",
        ).decode().strip()

    def test_conflicts_and_unsafe_states_fail_closed(self) -> None:
        cases = ("missing-classic", "dirty-classic", "duplicate-source", "archive")
        for case in cases:
            with self.subTest(case=case):
                temporary = tempfile.TemporaryDirectory(dir=self.root)
                try:
                    wrapper = Path(temporary.name) / "wrapper"
                    wrapper.mkdir()
                    workspace = Path(temporary.name) / "workspace"
                    with mock.patch.dict(
                        os.environ, {"ATRINIK_WORKSPACE_DIR": str(workspace)}
                    ):
                        paths = Paths.discover(wrapper)
                        paths.ensure()
                        original_wrapper = self.wrapper
                        original_paths = self.paths
                        self.wrapper = wrapper
                        self.paths = paths
                        source = self.make_repository("client", "client")
                        if case != "missing-classic":
                            classic = self.make_classic({"client": source})
                            if case == "dirty-classic":
                                (classic / "untracked.txt").write_text("dirty\n")
                        if case == "duplicate-source":
                            duplicate = wrapper / "legacy-client"
                            command("git", "clone", str(source), str(duplicate), cwd=wrapper)
                            command(
                                "git",
                                "remote",
                                "set-url",
                                "origin",
                                "https://github.com/atrinik/legacy-client.git",
                                cwd=duplicate,
                            )
                        if case == "archive":
                            conflict = (
                                workspace
                                / "archive"
                                / "classic-migration"
                                / "repositories"
                                / "client"
                            )
                            conflict.mkdir(parents=True)
                        result = RepositoryMigration(
                            wrapper,
                            paths,
                            self.manifest,
                            paths.workspace / "physical-repository-layout.lock",
                            lambda _transitions=None: None,
                        ).execute("apply")
                        self.assertEqual(result["status"], "refused")
                        self.assertTrue(result["refusals"])
                        self.assertTrue(source.is_dir())
                        self.wrapper = original_wrapper
                        self.paths = original_paths
                finally:
                    temporary.cleanup()

    def test_failure_rolls_back_worktree_profile_and_archive(self) -> None:
        source = self.make_repository("client", "client")
        classic = self.make_classic({"client": source})
        (source / "tracked.txt").write_text("dirty\n", encoding="utf-8")
        profile = self.write_profile(
            "rollback", {"client": {"kind": "primary", "value": ""}}
        )
        before_profile = profile.read_bytes()
        before_status = self.status_bytes(source)
        migration = self.migration()

        with mock.patch.object(
            RepositoryMigration,
            "_archive_source",
            side_effect=WorkspaceError("injected archive failure"),
        ):
            with self.assertRaisesRegex(WorkspaceError, "injected archive failure"):
                migration.execute("apply")

        self.assertTrue(source.is_dir())
        self.assertEqual(self.status_bytes(source), before_status)
        self.assertEqual(profile.read_bytes(), before_profile)
        self.assertEqual(self.status_bytes(classic), b"")
        self.assertFalse((self.paths.worktrees / "classic").exists())
        self.assertFalse(migration.pending_path.exists())
        self.assertFalse(migration.record_path.exists())
        refs = command("git", "for-each-ref", "--format=%(refname)", cwd=classic).decode()
        self.assertNotIn("refs/heads/migration/", refs)
        self.assertNotIn("refs/heads/archive/local-migration/", refs)

    def test_interrupted_journal_is_verified_rolled_back_and_reapplied(self) -> None:
        source = self.make_repository("client", "client")
        self.make_classic({"client": source})
        (source / "tracked.txt").write_text("interrupted dirty\n", encoding="utf-8")
        profile = self.write_profile(
            "interrupted", {"client": {"kind": "primary", "value": ""}}
        )
        before_profile = profile.read_bytes()
        before_status = self.status_bytes(source)
        migration = self.migration()
        inspection = migration._inspect()
        self.assertEqual(inspection.plan["status"], "ready")
        assert inspection.classic is not None
        migrated = next(
            worktree
            for source_value in inspection.sources
            for worktree in source_value.worktrees
            if worktree.destination is not None
        )
        action = inspection.profiles[0]
        migration.pending_path.parent.mkdir(parents=True, exist_ok=True)
        migration._durable_atomic_json(
            migration.pending_path,
            migration._pending_value(inspection),
        )
        migration._create_classic_worktree(inspection.classic, migrated)
        migration._exchange_profile(action)
        migration._archive_source(inspection.sources[0])
        self.assertFalse(source.exists())
        self.assertNotEqual(profile.read_bytes(), before_profile)

        result = self.migration().execute("apply")

        self.assertEqual(result["status"], "applied")
        archive = Path(result["sources"][0]["archive"])
        self.assertEqual(self.status_bytes(archive), before_status)
        self.assertFalse(self.migration().pending_path.exists())
        audit = self.migration().execute("audit")
        self.assertEqual(audit["status"], "complete", audit)

    def test_inert_content_state_build_logs_and_scenarios_are_unchanged(self) -> None:
        source = self.make_repository("client", "client")
        self.make_classic({"client": source})
        roots = (
            self.wrapper / "content",
            self.wrapper / "content-1x",
            self.paths.builds,
            self.paths.state,
            self.paths.scenarios,
            self.paths.topologies,
        )
        sentinels: dict[Path, tuple[bytes, tuple[int, int]]] = {}
        for index, root in enumerate(roots):
            root.mkdir(parents=True, exist_ok=True)
            sentinel = root / f"sentinel-{index}.bin"
            sentinel.write_bytes(b"preserve\x00" + bytes([index]))
            sentinels[sentinel] = (
                sentinel.read_bytes(),
                (sentinel.stat().st_dev, sentinel.stat().st_ino),
            )

        result = self.migration().execute("apply")

        self.assertEqual(result["status"], "applied")
        for path, (value, identity) in sentinels.items():
            self.assertEqual(path.read_bytes(), value)
            self.assertEqual((path.stat().st_dev, path.stat().st_ino), identity)

    def test_live_topology_and_lock_refuse_without_writes(self) -> None:
        source = self.make_repository("client", "client")
        self.make_classic({"client": source})
        topology = self.paths.topologies / "live"
        topology.mkdir(parents=True)
        lease_fd = os.open(
            topology / "process-tree.lease", os.O_RDWR | os.O_CREAT, 0o600
        )
        fcntl.flock(lease_fd, fcntl.LOCK_EX | fcntl.LOCK_NB)
        generation = "a" * 64
        lease = initialize_lease(lease_fd, generation)
        (topology / "status.json").write_text(
            json.dumps(
                {
                    "schema_version": 1,
                    "name": "live",
                    "profile": "classic",
                    "dependencies": [],
                    "state": None,
                    "build_root": str(self.paths.builds / "live"),
                    "resolved": {},
                    "endpoint": None,
                    "ready": False,
                    "started_at": "now",
                    "stopped_at": None,
                    "control": {
                        "socket": str(control_socket_path(topology, generation)),
                        "generation": generation,
                        "lease": lease,
                    },
                    "supervisor": {
                        "pid": 123,
                        "start_time": "1",
                        "generation": generation,
                    },
                    "services": {},
                }
            )
            + "\n",
            encoding="utf-8",
        )

        try:
            with mock.patch.object(
                migration_module, "process_matches", return_value=True
            ):
                result = self.migration().execute("apply")
        finally:
            os.close(lease_fd)

        self.assertEqual(result["status"], "refused")
        self.assertIn("live_topology", {row["code"] for row in result["refusals"]})
        self.assertTrue(source.is_dir())

        lock_path = self.workspace / "repository-layout.lock"
        descriptor = os.open(lock_path, os.O_RDWR | os.O_CREAT, 0o600)
        with os.fdopen(descriptor, "a+") as lock:
            fcntl.flock(lock, fcntl.LOCK_EX | fcntl.LOCK_NB)
            with mock.patch.object(
                migration_module,
                "process_matches",
                return_value=False,
            ):
                locked = self.migration().execute("apply")
        self.assertEqual(locked["status"], "refused")
        self.assertIn(
            "repository_layout_busy",
            {row["code"] for row in locked["refusals"]},
        )
        self.assertTrue(source.is_dir())

    def test_apply_refuses_when_writer_admission_is_held(self) -> None:
        source = self.make_repository("client", "client")
        self.make_classic({"client": source})
        layout = self.workspace / "repository-layout.lock"

        with exclusive_lock(
            layout_writer_intent_path(layout), "competing writer admission"
        ):
            result = self.migration().execute("apply")

        self.assertEqual(result["status"], "refused")
        self.assertIn(
            "repository_layout_busy",
            {row["code"] for row in result["refusals"]},
        )
        self.assertTrue(source.is_dir())

    def test_corrupt_pending_journal_refuses_and_audit_reports_incomplete(self) -> None:
        source = self.make_repository("client", "client")
        self.make_classic({"client": source})
        migration = self.migration()
        migration.pending_path.parent.mkdir(parents=True)
        migration.pending_path.write_text("{}\n", encoding="utf-8")

        result = migration.execute("apply")
        audit = migration.execute("audit")

        self.assertEqual(result["status"], "refused")
        self.assertIn("pending_migration", {row["code"] for row in result["refusals"]})
        self.assertEqual(audit["status"], "incomplete")
        self.assertTrue(source.is_dir())


if __name__ == "__main__":
    unittest.main()
