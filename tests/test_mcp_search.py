"""Synthetic acceptance tests for revision-aware bounded MCP source search."""

from __future__ import annotations

import tempfile
import threading
import time
import unittest
from dataclasses import dataclass
from pathlib import Path
import subprocess
from unittest import mock

from atrinik_workspace import mcp_search
from atrinik_workspace.mcp_contract import ContractError, Coordinate, canonical_json
from atrinik_workspace.mcp_search import search


class _Probe:
    def __init__(self, coordinate: Coordinate) -> None:
        self.coordinate = coordinate

    def __call__(self) -> Coordinate:
        return self.coordinate


class _Adapter:
    identity = "fixture-language-index-v1"

    def search(self, **values):
        self.values = values
        return [{"path": "src/code.rs", "line": 1, "column": 4, "symbol": "Packet", "language": "rust"}]


@dataclass(frozen=True)
class _Snapshot:
    coordinate: Coordinate
    root: Path
    identity: dict[str, object]
    metadata: dict[str, object]
    probe: _Probe

    def assert_current(self) -> None:
        if self.probe() != self.coordinate:
            raise ContractError("STALE_COORDINATE", "snapshot identity changed")


class SearchTest(unittest.TestCase):
    def setUp(self) -> None:
        self.temporary = tempfile.TemporaryDirectory()
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name)
        self.one = self.root / "one"
        self.two = self.root / "two"
        for root in (self.one, self.two):
            (root / "src").mkdir(parents=True)
        (self.one / "src" / "code.rs").write_text(
            "pub struct Packet;\nlet token=fixture-secret;\nPacket\n", encoding="utf-8"
        )
        (self.two / "src" / "code.go").write_text(
            "package fixture\ntype Packet struct{}\n", encoding="utf-8"
        )
        for root in (self.one, self.two):
            subprocess.run(
                ["git", "init", "-q", "-b", "main"],
                cwd=root,
                check=True,
            )
            subprocess.run(["git", "add", "."], cwd=root, check=True)
        self.coordinate_one = Coordinate(
            "atrinik/one", "main", "1" * 40, "main", "4" * 64
        )
        self.coordinate_two = Coordinate("atrinik/two", "review", "2" * 40, "review-2", "3" * 64)
        self.probe_one = _Probe(self.coordinate_one)
        self.probe_two = _Probe(self.coordinate_two)
        self.snapshots = [
            self.snapshot(self.one, "one", self.coordinate_one, self.probe_one),
            self.snapshot(self.two, "two", self.coordinate_two, self.probe_two),
        ]

    @staticmethod
    def snapshot(root, component, coordinate, probe, adapter=None):
        del adapter
        return _Snapshot(
            coordinate=coordinate,
            root=root,
            identity={"manifest": "fixture-v1", "authorization": "fixture-reader"},
            metadata={
                "component": component,
                "checkout": component,
                "profile": "default",
                "stack": "default",
                "roles": [component],
                "generation": "replacement",
                "owner": f"atrinik/{component}",
                "license": "MIT",
                "build": "none",
                "source": ".",
            },
            probe=probe,
        )

    @staticmethod
    def request(mode="exact", query="Packet", page_size=50, cursor=None, timeout_ms=1000):
        return {
            "mode": mode,
            "query": query,
            "case_sensitive": True,
            "page_size": page_size,
            "cursor": cursor,
            "timeout_ms": timeout_ms,
        }

    def test_dirty_search_rejects_untracked_content_and_match_oracles(self):
        subprocess.run(["git", "add", "src/code.rs"], cwd=self.one, check=True)
        (self.one / "src" / "code.rs").write_text("DirtyTrackedMarker\n", encoding="utf-8")
        tracked = search(self.request(query="DirtyTrackedMarker"), [self.snapshots[0]],
                         authorization_identity="fixture-reader")
        self.assertEqual(len(tracked["items"]), 1)
        (self.one / "untracked.txt").write_text("UntrackedMarker\n", encoding="utf-8")
        for mode, query in (("exact", "UntrackedMarker"), ("regex", "Untracked.*"),
                            ("path", "untracked"), ("filename", "untracked")):
            with self.subTest(mode=mode), self.assertRaisesRegex(ContractError, "STALE_COORDINATE"):
                search(self.request(mode=mode, query=query), [self.snapshots[0]],
                       authorization_identity="fixture-reader")

    def test_credential_paths_are_excluded_before_open_and_matching(self):
        paths = (".env.local", "credentials.json", "secrets.toml", "passwords.txt",
                 "private.key", "certificate.pem", "bundle.p12", "CREDENTIALS.JSON",
                 "nested/secrets.toml", "nested/credentials.json/source.txt")
        for path in paths:
            target = self.one / path
            target.parent.mkdir(parents=True, exist_ok=True)
            target.write_text("CredentialMarker\n", encoding="utf-8")
        subprocess.run(["git", "add", "."], cwd=self.one, check=True)
        original = mcp_search._open_regular
        opened = []

        def observe(descriptor, path):
            opened.append(path)
            return original(descriptor, path)

        with mock.patch.object(mcp_search, "_open_regular", side_effect=observe):
            for mode, query in (("exact", "CredentialMarker"), ("regex", "Credential.*"),
                                ("path", "credentials"), ("filename", "private.key")):
                with self.subTest(mode=mode):
                    result = search(self.request(mode=mode, query=query), [self.snapshots[0]],
                                    authorization_identity="fixture-reader")
                    self.assertEqual(result["items"], [])
        self.assertFalse(set(opened) & set(paths))
        for path in paths:
            for mode in ("history", "blame"):
                with self.subTest(path=path, mode=mode), self.assertRaisesRegex(ContractError, "FORBIDDEN"):
                    search({**self.request(mode=mode, query=""), "path": path, "provenance": True},
                           [self.snapshots[0]], authorization_identity="fixture-reader")
            self.assertIsNone(mcp_search._historical_component_path(self.snapshots[0], path))

    def test_server_search_cannot_reveal_excluded_dirty_or_untracked_bytes(self):
        from atrinik_workspace.mcp_server import ContextServer
        from tests import test_mcp_context as fixture
        from tests.test_mcp_server import request

        fixture.ContextFixture.setUp(self)
        server = ContextServer(self.service)
        for name in (".env.local", "credentials.json", "private.key"):
            (self.root / name).write_text("CommittedCredentialMarker\n")
        fixture.git(self.root, "add", ".")
        fixture.git(self.root, "commit", "-m", "credential fixture")
        for name in (".env.local", "credentials.json", "private.key"):
            (self.root / name).write_text("DirtyCredentialMarker\n")
        (self.root / "sample.txt").write_text("TrackedMarker\n")
        for mode, query in (("exact", "DirtyCredentialMarker"), ("regex", "DirtyCredential.*"),
                            ("filename", "credentials"), ("path", ".env.local")):
            response = server.handle(request("tools/call", name="atrinik_search",
                                            arguments={"mode": mode, "query": query}))
            self.assertEqual(response["result"]["structuredContent"]["data"]["items"], [])
        response = server.handle(request("tools/call", name="atrinik_search",
                                        arguments={"mode": "exact", "query": "TrackedMarker"}))
        self.assertEqual(len(response["result"]["structuredContent"]["data"]["items"]), 1)
        for payload in ("UntrackedMarker", "ChangedUntrackedMarker"):
            (self.root / "untracked.txt").write_text(payload)
            response = server.handle(request("tools/call", name="atrinik_search",
                                            arguments={"mode": "exact", "query": payload}))
            self.assertEqual(response["error"]["data"]["code"], "STALE_COORDINATE")
            self.assertNotIn(payload, canonical_json(response).decode())

    def test_credential_component_source_is_rejected_before_inventory(self):
        scoped = _Snapshot(
            coordinate=self.coordinate_one, root=self.one, identity=self.snapshots[0].identity,
            metadata={**self.snapshots[0].metadata, "source": "credentials.json"},
            probe=self.probe_one,
        )
        with mock.patch.object(mcp_search, "_capture") as capture:
            with self.assertRaisesRegex(ContractError, "FORBIDDEN"):
                search(self.request(), [scoped], authorization_identity="fixture-reader")
        capture.assert_not_called()

    def test_exact_search_is_cross_repository_revision_qualified_and_compact(self):
        result = search(self.request(), self.snapshots, authorization_identity="fixture-reader")
        self.assertEqual(len(result["items"]), 3)
        self.assertEqual({item["repository"] for item in result["items"]}, {"atrinik/one", "atrinik/two"})
        self.assertTrue(all(len(item["commit"]) == 40 for item in result["items"]))
        self.assertTrue(all(item["resource_uri"].startswith("atrinik://") for item in result["items"]))
        self.assertLessEqual(len(canonical_json(result)), 32 * 1024)
        self.assertEqual(result["freshness"]["state"], "dirty")

    def test_secret_assignments_are_redacted_from_snippets(self):
        result = search(
            self.request(query="token="), [self.snapshots[0]], authorization_identity="fixture-reader"
        )
        rendered = canonical_json(result).decode()
        self.assertNotIn("fixture-secret", rendered)
        self.assertIn("redacted", rendered)

    def test_path_and_filename_search_use_safe_regular_files_only(self):
        (self.one / "docs").mkdir()
        (self.one / "docs" / "Packet Guide.md").write_text("fixture", encoding="utf-8")
        outside = self.root / "outside.txt"
        outside.write_text("Packet", encoding="utf-8")
        (self.one / "linked.txt").symlink_to(outside)
        (self.one / "secrets").mkdir()
        (self.one / "secrets" / "Packet.txt").write_text("fixture", encoding="utf-8")
        (self.one / ".cache").mkdir()
        (self.one / ".cache" / "Packet.txt").write_text("fixture", encoding="utf-8")
        (self.one / "Packet.zip").write_text("fixture", encoding="utf-8")

        subprocess.run(["git", "add", "docs"], cwd=self.one, check=True)
        path_result = search(
            self.request(mode="path", query="docs/Packet"),
            [self.snapshots[0]],
            authorization_identity="fixture-reader",
        )
        filename_result = search(
            self.request(mode="filename", query="Packet Guide"),
            [self.snapshots[0]],
            authorization_identity="fixture-reader",
        )
        self.assertEqual([item["path"] for item in path_result["items"]], ["docs/Packet Guide.md"])
        self.assertEqual([item["path"] for item in filename_result["items"]], ["docs/Packet Guide.md"])
        all_paths = search(
            self.request(mode="path", query="Packet"),
            [self.snapshots[0]],
            authorization_identity="fixture-reader",
        )
        self.assertNotIn("secrets/Packet.txt", {item["path"] for item in all_paths["items"]})
        self.assertNotIn("linked.txt", {item["path"] for item in all_paths["items"]})
        self.assertNotIn(".cache/Packet.txt", {item["path"] for item in all_paths["items"]})
        self.assertNotIn("Packet.zip", {item["path"] for item in all_paths["items"]})

    def test_pagination_is_deterministic_and_bound_to_full_identity(self):
        for index in range(75):
            (self.one / "src" / f"record-{index:03}.txt").write_text("Needle\n", encoding="utf-8")
        subprocess.run(["git", "add", "src"], cwd=self.one, check=True)
        request = self.request(query="Needle", page_size=17)
        first = search(request, [self.snapshots[0]], authorization_identity="fixture-reader")
        paths = [item["path"] for item in first["items"]]
        cursor = first["pagination"]["next_cursor"]
        while cursor is not None:
            page = search(
                {**request, "cursor": cursor},
                [self.snapshots[0]],
                authorization_identity="fixture-reader",
            )
            paths.extend(item["path"] for item in page["items"])
            cursor = page["pagination"]["next_cursor"]
        self.assertEqual(len(paths), 75)
        self.assertEqual(len(paths), len(set(paths)))

        for changed in (
            ({**request, "cursor": first["pagination"]["next_cursor"], "query": "Other"}, "fixture-reader"),
            ({**request, "cursor": first["pagination"]["next_cursor"]}, "another-reader"),
        ):
            with self.assertRaisesRegex(ContractError, "STALE_CURSOR"):
                search(changed[0], [self.snapshots[0]], authorization_identity=changed[1])

    def test_revision_change_fails_before_search_and_after_scan(self):
        self.probe_one.coordinate = Coordinate("atrinik/one", "main", "4" * 40, "main", None)
        with self.assertRaisesRegex(ContractError, "STALE_COORDINATE"):
            search(self.request(), [self.snapshots[0]], authorization_identity="fixture-reader")

        self.probe_one.coordinate = self.coordinate_one
        original = self.probe_one
        calls = 0

        def changing_probe():
            nonlocal calls
            calls += 1
            return self.coordinate_one if calls == 1 else Coordinate(
                "atrinik/one", "main", "5" * 40, "main", None
            )

        changed = self.snapshot(self.one, "one", self.coordinate_one, changing_probe)
        with self.assertRaisesRegex(ContractError, "STALE_COORDINATE"):
            search(self.request(), [changed], authorization_identity="fixture-reader")
        self.assertIsNotNone(original)

    def test_regex_validation_cancellation_and_request_bounds(self):
        with self.assertRaisesRegex(ContractError, "INVALID_ARGUMENT"):
            search(
                self.request(mode="regex", query="["),
                self.snapshots,
                authorization_identity="fixture-reader",
            )
        cancelled = threading.Event()
        cancelled.set()
        with self.assertRaisesRegex(ContractError, "CANCELLED"):
            search(
                self.request(),
                self.snapshots,
                authorization_identity="fixture-reader",
                cancellation=cancelled,
            )
        with self.assertRaisesRegex(ContractError, "LIMIT_EXCEEDED"):
            search(
                self.request(page_size=51), self.snapshots, authorization_identity="fixture-reader"
            )
        with self.assertRaisesRegex(ContractError, "INVALID_ARGUMENT"):
            search(
                {**self.request(), "root": str(self.one)},
                self.snapshots,
                authorization_identity="fixture-reader",
            )

    def test_language_queries_require_and_use_owned_adapter(self):
        with self.assertRaisesRegex(ContractError, "UNSUPPORTED_OPERATION"):
            search(
                self.request(mode="symbols"),
                [self.snapshots[0]],
                authorization_identity="fixture-reader",
            )
        adapter = _Adapter()
        snapshot = self.snapshot(self.one, "one", self.coordinate_one, self.probe_one, adapter)
        result = search(
            self.request(mode="references"),
            [snapshot],
            authorization_identity="fixture-reader",
            language_adapters={"atrinik/one": adapter},
        )
        self.assertEqual(result["items"][0]["symbol"], "Packet")
        self.assertEqual(adapter.values["mode"], "references")

    def test_symlink_root_duplicate_repo_and_secret_bearing_paths_fail_closed(self):
        linked_root = self.root / "linked-root"
        linked_root.symlink_to(self.one, target_is_directory=True)
        linked = self.snapshot(linked_root, "one", self.coordinate_one, self.probe_one)
        with self.assertRaisesRegex(ContractError, "FORBIDDEN"):
            search(self.request(), [linked], authorization_identity="fixture-reader")
        with self.assertRaisesRegex(ContractError, "duplicate source"):
            search(
                self.request(),
                [self.snapshots[0], self.snapshots[0]],
                authorization_identity="fixture-reader",
            )
        (self.one / "token=do-not-publish.txt").write_text("Packet", encoding="utf-8")
        result = search(
            self.request(mode="path", query="token"),
            [self.snapshots[0]],
            authorization_identity="fixture-reader",
        )
        self.assertEqual(result["items"], [])
        self.assertNotIn("do-not-publish", canonical_json(result).decode())

    def test_ripgrep_configuration_and_process_environment_are_not_inherited(self):
        preprocessor = self.root / "preprocessor"
        preprocessor.write_text("#!/bin/sh\nprintf 'outside replacement\\n'\n", encoding="utf-8")
        preprocessor.chmod(0o700)
        configuration = self.root / "ripgrep.conf"
        configuration.write_text(f"--pre={preprocessor}\n", encoding="utf-8")
        with mock.patch.dict(
            "os.environ",
            {"RIPGREP_CONFIG_PATH": str(configuration), "TOKEN": "do-not-inherit"},
            clear=False,
        ):
            result = search(
                self.request(query="Packet"),
                [self.snapshots[0]],
                authorization_identity="fixture-reader",
            )
        self.assertEqual(len(result["items"]), 2)
        self.assertNotIn("outside replacement", canonical_json(result).decode())

    def test_deadline_is_checked_after_revision_probe(self):
        class SlowProbe(_Probe):
            def __call__(self):
                time.sleep(0.03)
                return super().__call__()

        slow = self.snapshot(
            self.one, "one", self.coordinate_one, SlowProbe(self.coordinate_one)
        )
        with self.assertRaisesRegex(ContractError, "TIMEOUT"):
            search(
                self.request(timeout_ms=10),
                [slow],
                authorization_identity="fixture-reader",
            )

    def test_deterministic_inventory_cap_does_not_skip_between_pages(self):
        for index in range(1001):
            (self.one / f"file-{index:04}.txt").write_text("fixture", encoding="utf-8")
        subprocess.run(["git", "add", "."], cwd=self.one, check=True)
        request = self.request(mode="filename", query="file-", page_size=1)
        first = search(request, [self.snapshots[0]], authorization_identity="fixture-reader")
        second = search(
            {**request, "cursor": first["pagination"]["next_cursor"]},
            [self.snapshots[0]],
            authorization_identity="fixture-reader",
        )
        self.assertEqual(first["items"][0]["path"], "file-0000.txt")
        self.assertEqual(second["items"][0]["path"], "file-0001.txt")
        self.assertTrue(first["incomplete"])

    def test_component_source_subdirectory_is_the_only_search_root(self):
        (self.one / "outside.txt").write_text("ScopedNeedle", encoding="utf-8")
        (self.one / "src" / "inside.txt").write_text("ScopedNeedle", encoding="utf-8")
        subprocess.run(["git", "add", "src"], cwd=self.one, check=True)
        scoped = _Snapshot(
            coordinate=self.coordinate_one,
            root=self.one,
            identity={"manifest": "fixture-v1", "authorization": "fixture-reader"},
            metadata={**self.snapshots[0].metadata, "source": "src"},
            probe=self.probe_one,
        )
        result = search(
            self.request(query="ScopedNeedle"),
            [scoped],
            authorization_identity="fixture-reader",
        )
        self.assertEqual([item["path"] for item in result["items"]], ["inside.txt"])
        self.assertTrue(result["items"][0]["resource_uri"].endswith("/src/inside.txt"))

    def test_oversized_source_is_skipped_with_explicit_incomplete_state(self):
        (self.one / "src" / "huge.txt").write_bytes(b"Needle" + b"x" * (256 * 1024))
        result = search(
            self.request(query="Needle"),
            [self.snapshots[0]],
            authorization_identity="fixture-reader",
        )
        self.assertNotIn("huge.txt", {item["path"] for item in result["items"]})
        self.assertTrue(result["incomplete"])
        self.assertTrue(result["truncation"]["records"])

    def test_synthetic_known_answers_preserve_physical_owner_mapping(self):
        domains = {
            "classic": "atrinik/classic",
            "replacement": "atrinik/server",
            "website": "atrinik/website",
            "shared-resource": "atrinik/resources",
            "provenance": "atrinik/atrinik",
            "content": "atrinik/content",
            "publisher": "atrinik/classic",
        }
        snapshots = []
        for index, (component, repository) in enumerate(domains.items(), 1):
            root = self.root / f"known-{component}"
            root.mkdir()
            subprocess.run(
                ["git", "init", "-q", "-b", "main"], cwd=root, check=True
            )
            (root / "answer.txt").write_text("KnownAnswerMarker\n", encoding="utf-8")
            subprocess.run(["git", "add", "answer.txt"], cwd=root, check=True)
            coordinate = Coordinate(
                repository,
                "main",
                f"{index:x}" * 40,
                f"fixture-{component}",
                "e" * 64,
            )
            snapshots.append(
                _Snapshot(
                    coordinate=coordinate,
                    root=root,
                    identity={"manifest": "known-answer-v1", "domain": component},
                    metadata={
                        "component": component,
                        "checkout": component,
                        "profile": "classic" if component in {"classic", "publisher"} else "default",
                        "stack": "classic" if component in {"classic", "publisher"} else "default",
                        "roles": [component],
                        "generation": "classic" if component in {"classic", "publisher"} else "replacement",
                        "owner": repository,
                        "license": "MIT",
                        "build": "classic-server" if component == "publisher" else "none",
                        "source": ".",
                    },
                    probe=_Probe(coordinate),
                )
            )
        result = search(
            self.request(query="KnownAnswerMarker"),
            snapshots,
            authorization_identity="fixture-reader",
        )
        mappings = {(item["component"], item["owner"]) for item in result["items"]}
        self.assertEqual(mappings, set(domains.items()))
        self.assertEqual(len(result["items"]), len(domains))

    def test_distinct_logical_components_may_share_one_physical_coordinate(self):
        (self.one / "client").mkdir()
        (self.one / "server").mkdir()
        (self.one / "client" / "answer.txt").write_text("SharedHeadMarker\n", encoding="utf-8")
        (self.one / "server" / "answer.txt").write_text("SharedHeadMarker\n", encoding="utf-8")
        subprocess.run(["git", "add", "client", "server"], cwd=self.one, check=True)
        snapshots = []
        for component in ("classic-client", "classic-server"):
            source = component.removeprefix("classic-")
            snapshots.append(
                _Snapshot(
                    coordinate=self.coordinate_one,
                    root=self.one,
                    identity={"manifest": "fixture-v1", "selection": component},
                    metadata={
                        **self.snapshots[0].metadata,
                        "component": component,
                        "source": source,
                    },
                    probe=self.probe_one,
                )
            )
        result = search(
            self.request(query="SharedHeadMarker"),
            snapshots,
            authorization_identity="fixture-reader",
        )
        self.assertEqual(
            {item["component"] for item in result["items"]},
            {"classic-client", "classic-server"},
        )

    def test_content_cap_is_stable_across_parallelizable_ripgrep_input(self):
        for file_index in range(32):
            payload = "".join(
                f"StableCapMarker {file_index:02}-{line_index:02}\n"
                for line_index in range(40)
            )
            (self.one / "src" / f"many-{file_index:02}.txt").write_text(
                payload, encoding="utf-8"
            )

        subprocess.run(["git", "add", "src"], cwd=self.one, check=True)

        def capture_keys():
            records, incomplete = mcp_search._content_records(
                self.snapshots[0],
                "exact",
                "StableCapMarker",
                True,
                time.monotonic() + 2,
                None,
            )
            self.assertTrue(incomplete)
            self.assertEqual(len(records), 1000)
            return [(record["path"], record["line"]) for record in records]

        self.assertEqual(capture_keys(), capture_keys())

    def test_literal_backslash_filename_is_rejected_without_duplicate_mapping(self):
        (self.one / "src\\code.rs").write_text("Packet\n", encoding="utf-8")
        result = search(
            self.request(query="Packet"),
            [self.snapshots[0]],
            authorization_identity="fixture-reader",
        )
        self.assertEqual(
            [item["path"] for item in result["items"]],
            ["src/code.rs", "src/code.rs"],
        )

    def test_shallow_history_and_blame_fail_closed_in_clone_and_worktree(self):
        def git(repository, *arguments):
            return subprocess.run(
                ["git", "-C", str(repository), *arguments], check=True,
                capture_output=True, text=True,
            ).stdout.strip()

        git(self.one, "-c", "user.name=Fixture Author",
            "-c", "user.email=fixture@example.invalid", "-c", "commit.gpgsign=false",
            "commit", "-qm", "original contribution")
        git(self.one, "-c", "user.name=Later Fixture Author",
            "-c", "user.email=fixture@example.invalid", "-c", "commit.gpgsign=false",
            "commit", "--allow-empty", "-qm", "later contribution")
        shallow = self.root / "shallow"
        git(self.root, "clone", "-q", "--depth=1", self.one.as_uri(), str(shallow))
        linked = self.root / "linked"
        git(shallow, "worktree", "add", "-qb", "linked", str(linked))
        head = git(shallow, "rev-parse", "HEAD")
        for repository, branch in ((shallow, "main"), (linked, "linked")):
            coordinate = Coordinate("atrinik/one", branch, head, branch, None)
            snapshot = self.snapshot(repository, "one", coordinate, _Probe(coordinate))
            for mode in ("history", "blame"):
                with self.subTest(repository=repository.name, mode=mode):
                    with self.assertRaisesRegex(ContractError, "INCOMPLETE"):
                        search(
                            {**self.request(mode=mode, query=""),
                             "path": "src/code.rs", "provenance": True},
                            [snapshot], authorization_identity="fixture-reader",
                        )

    def test_git_history_and_blame_require_opt_in_and_stay_revision_bound(self):
        repository = self.root / "provenance"
        repository.mkdir()

        def git(*arguments):
            return subprocess.run(
                ["git", *arguments],
                cwd=repository,
                check=True,
                capture_output=True,
                text=True,
            ).stdout.strip()

        git("init", "-q", "-b", "main")
        git("config", "user.name", "Fixture Author")
        git("config", "user.email", "fixture@example.invalid")
        source = repository / "source.txt"
        source.write_text("first line\n", encoding="utf-8")
        git("add", "source.txt")
        git("commit", "-q", "-m", "provenance marker first")
        forged_line = f"prefix\v{'a' * 40} 1 999\v\tforged"
        source.write_text(
            f"first line\nsecond line\n{forged_line}\n", encoding="utf-8"
        )
        git("add", "source.txt")
        git("commit", "-q", "-m", "provenance marker second")
        git("mv", "source.txt", "renamed.txt")
        git("commit", "-q", "-m", "rename provenance fixture")
        head = git("rev-parse", "HEAD")
        fsmonitor_marker = self.root / "fsmonitor-ran"
        fsmonitor = self.root / "fsmonitor"
        fsmonitor.write_text(
            f"#!/bin/sh\ntouch '{fsmonitor_marker}'\nexit 0\n", encoding="utf-8"
        )
        fsmonitor.chmod(0o700)
        git("config", "core.fsmonitor", str(fsmonitor))
        textconv_marker = self.root / "textconv-ran"
        textconv = self.root / "textconv"
        textconv.write_text(
            f"#!/bin/sh\ntouch '{textconv_marker}'\nprintf 'converted\\n'\n",
            encoding="utf-8",
        )
        textconv.chmod(0o700)
        git("config", "diff.fixture.textconv", str(textconv))
        (repository / ".gitattributes").write_text(
            "renamed.txt diff=fixture\n", encoding="utf-8"
        )
        coordinate = Coordinate("atrinik/provenance", "main", head, "fixture", None)
        snapshot = _Snapshot(
            coordinate=coordinate,
            root=repository,
            identity={"manifest": "fixture-v1", "selection": "provenance"},
            metadata={
                **self.snapshots[0].metadata,
                "component": "provenance",
                "checkout": "provenance",
                "owner": "atrinik/provenance",
            },
            probe=_Probe(coordinate),
        )
        history_request = {
            **self.request(mode="history", query="provenance marker"),
            "path": "renamed.txt",
            "provenance": True,
        }
        history = search(
            history_request, [snapshot], authorization_identity="fixture-reader"
        )
        self.assertEqual(len(history["items"]), 2)
        self.assertTrue(all(item["index_source"] == "git-log" for item in history["items"]))
        self.assertTrue(all(item["snapshot_commit"] == head for item in history["items"]))
        self.assertTrue(
            all(item["historical_path"] == "source.txt" for item in history["items"])
        )
        self.assertTrue(
            all(item["resource_uri"].endswith("/source.txt") for item in history["items"])
        )
        self.assertFalse(fsmonitor_marker.exists())

        blame = search(
            {
                **self.request(mode="blame", query=""),
                "path": "renamed.txt",
                "line": 2,
                "provenance": True,
            },
            [snapshot],
            authorization_identity="fixture-reader",
        )
        self.assertEqual(len(blame["items"]), 1)
        self.assertEqual(blame["items"][0]["line"], 2)
        self.assertEqual(blame["items"][0]["snippet"], "second line")
        self.assertEqual(blame["items"][0]["index_source"], "git-blame")
        self.assertEqual(blame["items"][0]["historical_path"], "source.txt")
        self.assertTrue(blame["items"][0]["resource_uri"].endswith("/source.txt"))
        self.assertFalse(textconv_marker.exists())

        all_blame = search(
            {
                **self.request(mode="blame", query=""),
                "path": "renamed.txt",
                "provenance": True,
            },
            [snapshot],
            authorization_identity="fixture-reader",
        )
        self.assertEqual(len(all_blame["items"]), 3)
        self.assertNotIn(999, {item["line"] for item in all_blame["items"]})

        with self.assertRaisesRegex(ContractError, "UNSUPPORTED_OPERATION"):
            search(
                {**history_request, "provenance": False},
                [snapshot],
                authorization_identity="fixture-reader",
            )
        (repository / "untracked.txt").write_text("fixture", encoding="utf-8")
        with self.assertRaisesRegex(ContractError, "STALE_COORDINATE"):
            search(
                {**history_request, "path": "untracked.txt"},
                [snapshot],
                authorization_identity="fixture-reader",
            )
        (repository / "[ab].txt").write_text("fixture", encoding="utf-8")
        (repository / "a.txt").write_text("tracked", encoding="utf-8")
        git("add", "a.txt")
        git("commit", "-q", "-m", "add tracked pathspec neighbor")
        dirty_coordinate = Coordinate(
            coordinate.repository,
            coordinate.branch,
            coordinate.commit,
            coordinate.worktree,
            "f" * 64,
        )
        dirty_snapshot = _Snapshot(
            coordinate=dirty_coordinate,
            root=repository,
            identity={**snapshot.identity, "dirty": "f" * 64},
            metadata=snapshot.metadata,
            probe=_Probe(dirty_coordinate),
        )
        with self.assertRaisesRegex(ContractError, "STALE_COORDINATE"):
            search(
                {**history_request, "path": "[ab].txt"},
                [dirty_snapshot],
                authorization_identity="fixture-reader",
            )
        filter_marker = self.root / "filter-ran"
        filter_command = self.root / "filter-command"
        filter_command.write_text(
            f"#!/bin/sh\ntouch '{filter_marker}'\ncat\n", encoding="utf-8"
        )
        filter_command.chmod(0o700)
        git("config", "filter.unsafe.clean", str(filter_command))
        with self.assertRaisesRegex(ContractError, "FORBIDDEN"):
            search(
                {**history_request, "path": "renamed.txt"},
                [dirty_snapshot],
                authorization_identity="fixture-reader",
            )
        self.assertFalse(filter_marker.exists())

    def test_clean_search_rejects_assume_unchanged_hidden_edit(self):
        repository = self.root / "hidden-index"
        repository.mkdir()

        def git(*arguments):
            return subprocess.run(
                ["git", *arguments],
                cwd=repository,
                check=True,
                capture_output=True,
                text=True,
            ).stdout.strip()

        git("init", "-q", "-b", "main")
        git("config", "user.name", "Fixture Author")
        git("config", "user.email", "fixture@example.invalid")
        source = repository / "source.txt"
        source.write_text("CleanMarker\n", encoding="utf-8")
        git("add", "source.txt")
        git("commit", "-q", "-m", "add source")
        head = git("rev-parse", "HEAD")
        coordinate = Coordinate("atrinik/hidden", "main", head, "fixture", None)
        snapshot = _Snapshot(
            coordinate=coordinate,
            root=repository,
            identity={"manifest": "fixture-v1", "selection": "hidden"},
            metadata={
                **self.snapshots[0].metadata,
                "component": "hidden",
                "checkout": "hidden",
                "owner": "atrinik/hidden",
            },
            probe=_Probe(coordinate),
        )
        clean = search(
            self.request(query="CleanMarker"),
            [snapshot],
            authorization_identity="fixture-reader",
        )
        self.assertEqual(len(clean["items"]), 1)

        git("update-index", "--assume-unchanged", "source.txt")
        source.write_text("HiddenEdit\n", encoding="utf-8")
        with self.assertRaisesRegex(ContractError, "STALE_COORDINATE"):
            search(
                self.request(query="HiddenEdit"),
                [snapshot],
                authorization_identity="fixture-reader",
            )

    def test_descriptor_growth_and_clean_normalization_fail_bounded(self):
        growing = self.root / "growing.txt"
        growing.write_text("small", encoding="utf-8")
        descriptor = growing.open("rb")
        try:
            growing.write_bytes(b"x" * (256 * 1024 + 1))
            with self.assertRaisesRegex(ContractError, "STALE_COORDINATE"):
                mcp_search._descriptor_git_oid(
                    descriptor.fileno(), "sha1", time.monotonic() + 1, None
                )
        finally:
            descriptor.close()

        repository = self.root / "normalized"
        repository.mkdir()

        def git(*arguments):
            return subprocess.run(
                ["git", *arguments],
                cwd=repository,
                check=True,
                capture_output=True,
                text=True,
            ).stdout.strip()

        git("init", "-q", "-b", "main")
        git("config", "user.name", "Fixture Author")
        git("config", "user.email", "fixture@example.invalid")
        (repository / ".gitattributes").write_text(
            "*.txt text eol=crlf\n", encoding="utf-8"
        )
        source = repository / "source.txt"
        source.write_bytes(b"NormalizedMarker\r\n")
        git("add", ".gitattributes", "source.txt")
        git("commit", "-q", "-m", "add normalized source")
        self.assertEqual(git("status", "--porcelain"), "")
        head = git("rev-parse", "HEAD")
        coordinate = Coordinate("atrinik/normalized", "main", head, "fixture", None)
        snapshot = _Snapshot(
            coordinate=coordinate,
            root=repository,
            identity={"manifest": "fixture-v1", "selection": "normalized"},
            metadata={
                **self.snapshots[0].metadata,
                "component": "normalized",
                "checkout": "normalized",
                "owner": "atrinik/normalized",
            },
            probe=_Probe(coordinate),
        )
        with self.assertRaisesRegex(ContractError, "UNSUPPORTED_OPERATION"):
            search(
                self.request(query="NormalizedMarker"),
                [snapshot],
                authorization_identity="fixture-reader",
            )


if __name__ == "__main__":
    unittest.main()
