# Copyright 2026 The Atrinik Project
# SPDX-License-Identifier: MIT
from __future__ import annotations

import json
from pathlib import Path
import subprocess
import tempfile
import threading
import unittest
from unittest.mock import patch

from atrinik_workspace.mcp_context import ContextService, request_scope
from atrinik_workspace.mcp_contract import ContractError

REPOSITORY = Path(__file__).resolve().parents[1]


def git(path, *args):
    return subprocess.run(["git", "-C", str(path), *args], check=True, capture_output=True).stdout.decode().strip()


def repository(path, owner="atrinik/atrinik"):
    path.mkdir(parents=True, exist_ok=True)
    git(path, "init", "-b", "main")
    git(path, "config", "user.name", "Synthetic Fixture")
    git(path, "config", "user.email", "fixture@example.invalid")
    git(path, "remote", "add", "origin", "https://github.com/" + owner + ".git")
    (path / "AGENTS.md").write_text("Synthetic guidance. Run the owner's validation.\n")
    (path / "sample.txt").write_text("original\n")
    git(path, "add", ".")
    git(path, "commit", "-m", "fixture")


def long_changed_paths(root):
    directory = root
    for index in range(6):
        directory /= ("é" * 70 + '"' * 10 + str(index))
    directory.mkdir(parents=True)
    paths = [directory / f"changed-{index:02}.txt" for index in range(50)]
    for path in paths:
        path.write_text("original\n")
    git(root, "add", ".")
    git(root, "commit", "-m", "long path fixtures")
    for path in paths:
        path.write_text("changed\n")
    return {str(path.relative_to(root)) for path in paths}


class ContextFixture(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name) / "wrapper"
        repository(self.root)
        (self.root / "components.json").write_bytes((REPOSITORY / "components.json").read_bytes())
        git(self.root, "add", ".")
        git(self.root, "commit", "-m", "manifest")
        self.service = ContextService(self.root)

    def assertCode(self, code, operation):
        with self.assertRaises(ContractError) as caught:
            operation()
        self.assertEqual(caught.exception.code, code)

    def test_manifest_components_and_classic_share_one_checkout(self):
        manifest = json.loads((self.root / "components.json").read_bytes())
        for stack in ("default", "classic"):
            result = self.service.describe(stack, page_size=50)
            self.assertEqual({row["component"] for row in result["items"]}, set(manifest["stacks"][stack]["components"]))
        repository(self.root / "classic", "atrinik/classic")
        for directory in ("client", "server", "editor", "protocol", "libatrinik"):
            (self.root / "classic" / directory).mkdir()
        snapshots = [self.service.resolve("classic", component=role) for role in
                     ("classic-client", "classic-server", "classic-editor", "classic-protocol", "classic-libatrinik")]
        self.assertEqual(len({item.coordinate.worktree for item in snapshots}), 1)
        self.assertEqual(len({item.coordinate.commit for item in snapshots}), 1)
        self.assertEqual(self.service.resolve("classic", role="client").metadata["component"], "classic-client")
        self.assertCode("NOT_FOUND", lambda: self.service.resolve("default", component="classic-client"))

    def test_snapshot_dirty_head_authorization_and_manifest_invalidation(self):
        snapshot = self.service.resolve()
        self.assertEqual(snapshot.read("sample.txt"), b"original\n")
        (self.root / "sample.txt").write_text("changed\n")
        self.assertCode("STALE_COORDINATE", snapshot.assert_current)
        changed = self.service.resolve()
        self.assertIsNotNone(changed.coordinate.dirty_fingerprint)
        self.assertNotEqual(changed.identity, ContextService(self.root, "other").resolve().identity)
        git(self.root, "add", "sample.txt")
        git(self.root, "commit", "-m", "change")
        self.assertCode("STALE_COORDINATE", changed.assert_current)
        current = self.service.resolve()
        self.service.authorization_identity = "revoked"
        self.assertCode("STALE_COORDINATE", current.assert_current)

    def test_no_source_or_workspace_mutation(self):
        before = sorted(str(item.relative_to(self.root)) for item in self.root.rglob("*"))
        head = git(self.root, "rev-parse", "HEAD")
        self.service.describe()
        self.service.list_profiles()
        self.service.list_worktrees()
        self.service.resolve().read("AGENTS.md")
        self.service.guidance()
        self.service.changes()
        self.assertEqual(before, sorted(str(item.relative_to(self.root)) for item in self.root.rglob("*")))
        self.assertEqual(head, git(self.root, "rev-parse", "HEAD"))
        self.assertFalse((self.root / "workspace").exists())

    def test_reads_deny_untracked_symlink_traversal_and_secret_state(self):
        (self.root / "private.txt").write_text("SYNTHETIC_SENTINEL")
        snapshot = self.service.resolve()
        for path in ("private.txt", "../sample.txt", "/tmp/sample.txt", ".git/config", "workspace/state"):
            self.assertCode("FORBIDDEN", lambda path=path: snapshot.read(path))
        (self.root / "sample.txt").unlink()
        (self.root / "sample.txt").symlink_to("private.txt")
        self.assertCode("FORBIDDEN", lambda: self.service.resolve().read("sample.txt"))
        alias = self.root.parent / "alias"
        alias.symlink_to(self.root, target_is_directory=True)
        self.assertCode("NOT_FOUND", lambda: ContextService(alias))

    def test_pagination_over_300_registry_records_and_stale_cursor(self):
        original = self.service._registry(self.root)
        records = original[0] + [{"worktree": str(self.root.parent / f"review-{index:03}"),
                                  "HEAD": "a" * 40, "branch": f"refs/heads/review-{index:03}"}
                                 for index in range(300)]
        seen = []
        cursor = None
        with patch.object(self.service, "_registry", return_value=(records, "registry-v1")):
            while True:
                page = self.service.list_worktrees(page_size=50, cursor=cursor)
                seen.extend(item["worktree"] for item in page["items"])
                cursor = page["next_cursor"]
                if not cursor:
                    break
            initial = self.service.list_worktrees(page_size=50)
        self.assertEqual(len(seen), 301)
        self.assertEqual(len(set(seen)), 301)
        with patch.object(self.service, "_registry", return_value=(records, "registry-v2")):
            self.assertCode("STALE_CURSOR", lambda: self.service.list_worktrees(page_size=50, cursor=initial["next_cursor"]))

    def test_malformed_profile_does_not_hide_healthy_profiles(self):
        profiles = self.root / "workspace/profiles"
        profiles.mkdir(parents=True)
        (profiles / "broken.json").write_text("not json")
        page = self.service.list_profiles()
        self.assertTrue(page["incomplete"])
        self.assertIn("default", [row.get("name") for row in page["items"]])
        self.assertEqual(sum(row["incomplete"] for row in page["items"]), 1)

    def test_cancelled_and_detached_observations(self):
        event = threading.Event()
        event.set()
        with request_scope(event):
            self.assertCode("CANCELLED", self.service.resolve)
        git(self.root, "checkout", "--detach")
        self.assertEqual(self.service.resolve().coordinate.branch, "detached")

    def test_changed_paths_and_guidance_are_bounded_source_metadata(self):
        (self.root / "sample.txt").write_text("edited\n")
        (self.root / "untracked.txt").write_text("SYNTHETIC_SENTINEL")
        changes = self.service.changes()
        self.assertEqual([item["path"] for item in changes["items"]], ["sample.txt"])
        self.assertEqual(self.service.guidance()["guidance"], ["AGENTS.md"])
        self.assertNotIn("SYNTHETIC_SENTINEL", json.dumps(changes))

    def test_changed_path_pages_shrink_to_bytes_without_skips(self):
        expected = long_changed_paths(self.root)
        seen, cursor = [], None
        while True:
            page = self.service.changes(page_size=50, cursor=cursor)
            self.assertLessEqual(len(json.dumps(page).encode()), 32768)
            self.assertEqual(page["returned_records"], len(page["items"]))
            self.assertEqual(page["total_records"], len(expected))
            self.assertEqual(page["truncated"], page["next_cursor"] is not None)
            self.assertGreater(page["returned_records"], 0)
            self.assertLess(page["returned_records"], 50)
            seen.extend(item["path"] for item in page["items"])
            cursor = page["next_cursor"]
            if cursor is None:
                break
        self.assertEqual(set(seen), expected)
        self.assertEqual(len(seen), len(expected))
        first = self.service.changes(page_size=50)
        (self.root / "sample.txt").write_text("cursor invalidation\n")
        self.assertCode("STALE_CURSOR", lambda: self.service.changes(page_size=50, cursor=first["next_cursor"]))

    def test_origin_mismatch_fails_closed(self):
        git(self.root, "remote", "set-url", "origin", "https://example.invalid/other.git")
        self.assertCode("FORBIDDEN", self.service.resolve)

    def test_every_manifest_component_resolves_and_content_is_shared(self):
        manifest = self.service.manifest()
        for checkout in manifest.checkouts:
            repository(self.root / checkout.path, checkout.repository)
        for component in manifest.components:
            if component.source != ".":
                (self.root / component.checkout / component.source).mkdir(parents=True, exist_ok=True)
        for stack in manifest.stacks.values():
            for component in stack.components:
                observed = self.service.resolve(stack.name, component.name)
                self.assertEqual(observed.coordinate.repository, component.repository)
                self.assertEqual(observed.metadata["license"], component.license)
                self.assertEqual(observed.metadata["source"], component.source)
        replacement = self.service.resolve("default", "content")
        classic = self.service.resolve("classic", "content")
        self.assertEqual(replacement.coordinate, classic.coordinate)
        self.assertEqual(replacement.metadata["main_base_commit"], replacement.coordinate.commit)

    def test_no_follow_fifo_and_tracked_credentials(self):
        import os
        (self.root / ".env.local").write_text("SYNTHETIC_SECRET")
        git(self.root, "add", ".env.local")
        git(self.root, "commit", "-m", "synthetic restricted fixture")
        self.assertCode("FORBIDDEN", lambda: self.service.resolve().read(".env.local"))
        (self.root / "sample.txt").unlink()
        os.mkfifo(self.root / "sample.txt")
        self.assertCode("FORBIDDEN", lambda: self.service.resolve().read("sample.txt"))

    def test_deadline_kills_only_its_owned_git_process(self):
        from atrinik_workspace.mcp_context import _git
        import sys
        original = subprocess.Popen
        processes = []
        def delayed(*args, **kwargs):
            process = original([sys.executable, "-c", "import time; time.sleep(2)"],
                               stdout=subprocess.PIPE, stderr=subprocess.DEVNULL)
            processes.append(process)
            return process
        with patch("atrinik_workspace.mcp_context.subprocess.Popen", side_effect=delayed):
            with request_scope(timeout_ms=20):
                self.assertCode("TIMEOUT", lambda: _git(self.root, "rev-parse", "HEAD"))
        self.assertTrue(processes)
        self.assertTrue(all(process.poll() is not None for process in processes))

    def test_malformed_worktree_record_isolated_and_no_eager_checkout_read(self):
        from atrinik_workspace.mcp_context import _git
        raw = _git(self.root, "worktree", "list", "--porcelain", "-z")
        malformed = raw + b"worktree invalid\x00HEAD broken\x00\x00"
        with patch("atrinik_workspace.mcp_context._git", return_value=malformed):
            records, _ = self.service._registry(self.root)
        self.assertEqual(len(records), 2)
        self.assertEqual(records[0]["worktree"], str(self.root))
        self.assertEqual(records[1]["incomplete"], "true")

    def test_registry_names_never_open_mutable_payloads(self):
        workspace = self.root / "workspace"
        (workspace / "topologies/demo").mkdir(parents=True)
        (workspace / "topologies/demo/private.save").write_text("SYNTHETIC_SECRET")
        (workspace / "states.json").write_text(json.dumps({"schema_version": 1, "states": {"demo": "/not-opened/private-state"}}))
        for kind in ("topologies", "states"):
            result = self.service.list_profiles(kind=kind)
            self.assertEqual(result["items"][0]["name"], "demo")
            self.assertNotIn("SYNTHETIC_SECRET", json.dumps(result))
            self.assertNotIn("/not-opened", json.dumps(result))

    def test_hidden_index_changes_cannot_masquerade_as_clean_source(self):
        git(self.root, "update-index", "--assume-unchanged", "sample.txt")
        (self.root / "sample.txt").write_text("hidden change\n")
        self.assertCode("FORBIDDEN", lambda: self.service.resolve().read("sample.txt"))

    def test_whole_observation_fences_same_commit_branch_switch(self):
        from atrinik_workspace.mcp_context import _git
        changed = False
        def observe(root, *args, **kwargs):
            nonlocal changed
            value = _git(root, *args, **kwargs)
            if args == ("rev-parse", "--abbrev-ref", "HEAD") and not changed:
                changed = True
                git(root, "checkout", "-b", "review")
            return value
        with patch("atrinik_workspace.mcp_context._git", side_effect=observe):
            self.assertCode("STALE_COORDINATE", self.service.resolve)

    def test_whole_observation_fences_manifest_change(self):
        from atrinik_workspace.mcp_context import _git
        changed = False
        def observe(root, *args, **kwargs):
            nonlocal changed
            value = _git(root, *args, **kwargs)
            if args == ("rev-parse", "--abbrev-ref", "HEAD") and not changed:
                changed = True
                manifest = root / "components.json"
                manifest.write_bytes(manifest.read_bytes() + b"\n")
            return value
        with patch("atrinik_workspace.mcp_context._git", side_effect=observe):
            self.assertCode("STALE_COORDINATE", self.service.resolve)

    def test_replace_refs_cannot_substitute_clean_blob_identity(self):
        old = git(self.root, "rev-parse", "HEAD:sample.txt")
        replacement = subprocess.run(["git", "-C", str(self.root), "hash-object", "-w", "--stdin"],
                                     input=b"replacement\n", check=True, capture_output=True).stdout.decode().strip()
        git(self.root, "replace", old, replacement)
        self.assertEqual(self.service.resolve().read("sample.txt"), b"original\n")

    def test_inspection_never_executes_configured_clean_filters(self):
        sentinel = self.root / "filter-sentinel"
        (self.root / ".gitattributes").write_text("sample.txt filter=fixture\n")
        git(self.root, "add", ".gitattributes")
        git(self.root, "commit", "-m", "attributes fixture")
        git(self.root, "config", "filter.fixture.clean", "touch filter-sentinel; cat")
        (self.root / "sample.txt").write_text("changed to trigger clean filter\n")
        self.assertCode("FORBIDDEN", self.service.resolve)
        self.assertFalse(sentinel.exists())
