"""Executable, no-network model tests for the program publication contract."""

from __future__ import annotations

import copy
from datetime import datetime
import fcntl
import hashlib
import json
import os
from pathlib import Path
import re
import tempfile
import unittest
import unicodedata


class StopClosed(RuntimeError):
    """The modeled ledger cannot safely authorize a remote mutation."""


class CallPermit:
    """Ephemeral authority returned only by the durable in-flight transition."""

    def __init__(self, slot: str, generation: int) -> None:
        self.slot = slot
        self.generation = generation
        self.used = False


class ScanPermit:
    """One-use proof returned only by a complete bounded pagination pass."""

    def __init__(self, stream: str, evidence: dict[str, object]) -> None:
        self.stream = stream
        self.evidence = evidence
        self.seal = hashlib.sha256(
            json.dumps(evidence, sort_keys=True, separators=(",", ":")).encode()
        ).hexdigest()
        self.used = False


class ProgramLedgerModel:
    """Reference reducer with distinct persistence and remote-call boundaries."""

    MAX_LEDGER_BYTES = 8 * 1024 * 1024
    MAX_COLLECTION = 10_000
    MAX_STRING_BYTES = 65_536
    MAX_DEPTH = 16
    MAX_INTEGER = (1 << 64) - 1
    AUTO_SCAN = object()

    KEYS = {
        "generation", "self_inode", "lock", "previous_sha256", "authority", "graph",
        "next_graph", "comment", "create", "link", "leaf_snapshots", "observation",
    }

    def __init__(self) -> None:
        empty = {
            "phase": "none", "node": None, "prior": None,
            "created_at": None,
            "plan_observation": None, "arm_observation": None,
            "retry_observation": None,
        }
        self.record: dict[str, object] = {
            "generation": 0,
            "self_inode": 101,
            "lock": {"device": 1, "inode": 41},
            "previous_sha256": None,
            "authority": ["repo", "master", "goal", "actor"],
            "graph": ["leaf-1"],
            "next_graph": None,
            "comment": copy.deepcopy(empty),
            "create": copy.deepcopy(empty),
            "link": copy.deepcopy(empty),
            "leaf_snapshots": {"leaf-1": [1, "a" * 64]},
            "observation": {"comment": None, "child": None, "parent": None},
        }
        self.lock_inode = 41
        self.path_lock_inode = 41
        self.remote_calls = {"comment": 0, "create": 0, "link": 0}
        self.report_present = False

    @classmethod
    def canonical(cls, value: object) -> bytes:
        if not cls.value_is_bounded(value):
            raise StopClosed("canonical value violates type, NFC, or size bounds")
        try:
            text = json.dumps(
                value, ensure_ascii=False, allow_nan=False, sort_keys=True,
                separators=(",", ":"),
            )
        except (TypeError, ValueError, RecursionError) as error:
            raise StopClosed("canonical serialization failed") from error
        try:
            encoded = (text + "\n").encode("utf-8")
        except UnicodeEncodeError as error:
            raise StopClosed("canonical value contains a non-scalar string") from error
        if len(encoded) > cls.MAX_LEDGER_BYTES:
            raise StopClosed("canonical value exceeds its byte ceiling")
        return encoded

    @classmethod
    def decode(cls, raw: bytes) -> dict[str, object]:
        if not cls.raw_size_allowed(len(raw)):
            raise StopClosed("ledger exceeds its pre-parse byte ceiling")
        try:
            def unique_object(pairs: list[tuple[str, object]]) -> dict[str, object]:
                result: dict[str, object] = {}
                for key, item in pairs:
                    if key in result:
                        raise StopClosed("ledger contains a duplicate object key")
                    result[key] = item
                return result

            def bounded_integer(token: str) -> int:
                if len(token) > 20:
                    raise StopClosed("ledger integer token exceeds uint64")
                value = int(token)
                if value < 0 or value > cls.MAX_INTEGER:
                    raise StopClosed("ledger integer exceeds uint64")
                return value

            value = json.loads(
                raw, object_pairs_hook=unique_object, parse_int=bounded_integer
            )
        except (
            UnicodeDecodeError, UnicodeEncodeError, json.JSONDecodeError,
            RecursionError, ValueError,
        ) as error:
            raise StopClosed("ledger JSON is invalid") from error
        if not isinstance(value, dict) or not cls.value_is_bounded(value):
            raise StopClosed("canonical ledger exceeds its byte ceiling")
        canonical = cls.canonical(value)
        if len(canonical) > cls.MAX_LEDGER_BYTES or raw != canonical:
            raise StopClosed("retained ledger bytes are not canonical")
        return value

    @classmethod
    def raw_size_allowed(cls, size: object) -> bool:
        return (
            isinstance(size, int) and not isinstance(size, bool)
            and 0 <= size <= cls.MAX_LEDGER_BYTES
        )

    @classmethod
    def value_is_bounded(cls, value: object) -> bool:
        pending: list[tuple[object, int, str | None]] = [(value, 1, None)]
        while pending:
            current, depth, field = pending.pop()
            if depth > cls.MAX_DEPTH:
                return False
            if isinstance(current, str):
                if not cls.string_is_bounded(current) or (
                    field is not None and field.endswith("_path")
                    and not cls.path_is_bounded(current)
                ):
                    return False
            elif isinstance(current, dict):
                if len(current) > cls.MAX_COLLECTION:
                    return False
                for key, item in current.items():
                    if (
                        not isinstance(key, str)
                        or not cls.string_is_bounded(key)
                    ):
                        return False
                    pending.append((item, depth + 1, key))
            elif isinstance(current, list):
                if len(current) > cls.MAX_COLLECTION:
                    return False
                pending.extend((item, depth + 1, field) for item in current)
            elif current is not None:
                if type(current) is bool:
                    continue
                if type(current) is not int or not 0 <= current <= cls.MAX_INTEGER:
                    return False
        return True

    @classmethod
    def string_is_bounded(cls, value: str) -> bool:
        try:
            return (
                unicodedata.normalize("NFC", value) == value
                and len(value.encode("utf-8")) <= cls.MAX_STRING_BYTES
            )
        except UnicodeEncodeError:
            return False

    @staticmethod
    def path_is_bounded(value: object) -> bool:
        try:
            return isinstance(value, str) and len(value.encode("utf-8")) <= 4_096
        except UnicodeEncodeError:
            return False

    @staticmethod
    def valid_timestamp(value: object) -> bool:
        if not isinstance(value, str) or not re.fullmatch(
            r"\d{4}-\d{2}-\d{2}T\d{2}:\d{2}:\d{2}(?:\.\d+)?Z", value
        ):
            return False
        try:
            parsed = datetime.fromisoformat(value[:-1] + "+00:00")
        except ValueError:
            return False
        return parsed.tzinfo is not None and parsed.utcoffset().total_seconds() == 0

    @staticmethod
    def digest_is_valid(value: object) -> bool:
        return isinstance(value, str) and re.fullmatch(r"[0-9a-f]{64}", value) is not None

    def digest(self) -> str:
        return hashlib.sha256(self.canonical(self.record)).hexdigest()

    @staticmethod
    def coordinate(repository_node: str, master_node: str) -> str:
        payload = {
            "domain": "atrinik-program-delivery-coordinate",
            "master_node_id": master_node,
            "repository_node_id": repository_node,
            "schema_version": 1,
        }
        return hashlib.sha256(ProgramLedgerModel.canonical(payload)).hexdigest()

    @staticmethod
    def result_stream(results: list[dict[str, str]]) -> str:
        return hashlib.sha256(ProgramLedgerModel.canonical(results)).hexdigest()

    @classmethod
    def fresh(cls, lock_exists: bool, ledger_exists: bool) -> "ProgramLedgerModel":
        if lock_exists or ledger_exists:
            raise StopClosed("not a provably fresh coordinate")
        return cls()

    @classmethod
    def resume(
        cls, record: dict[str, object] | None, lock_inode: int | None,
        observed_inode: int | None, observed_sha256: str | None,
        expected_authority: list[str], expected_previous: str | None = None,
        report_present: bool = False,
        remote_calls: dict[str, int] | None = None,
    ) -> "ProgramLedgerModel":
        if record is None or lock_inode is None:
            raise StopClosed("ledger or stable lock was lost")
        if not isinstance(record, dict):
            raise StopClosed("ledger root is not an object")
        if not cls.value_is_bounded(record):
            raise StopClosed("ledger value exceeds a collection or string bound")
        self_inode = record.get("self_inode")
        lock = record.get("lock")
        if (
            set(record) != cls.KEYS
            or type(self_inode) is not int or self_inode <= 0
            or type(observed_inode) is not int or observed_inode <= 0
            or observed_inode != self_inode
            or type(lock_inode) is not int or lock_inode <= 0
            or not isinstance(lock, dict) or set(lock) != {"device", "inode"}
            or type(lock["device"]) is not int or lock["device"] < 0
            or type(lock["inode"]) is not int or lock["inode"] <= 0
        ):
            raise StopClosed("schema or inode corruption")
        if (
            not isinstance(record["authority"], list)
            or not isinstance(record["graph"], list)
            or any(not isinstance(item, str) or not item for item in record["graph"])
            or len(record["graph"]) != len(set(record["graph"]))
            or (
                record["next_graph"] is not None
                and (
                    not isinstance(record["next_graph"], list)
                    or any(
                        not isinstance(item, str) or not item
                        for item in record["next_graph"]
                    )
                    or len(record["next_graph"]) != len(set(record["next_graph"]))
                )
            )
            or not isinstance(record["leaf_snapshots"], dict)
            or any(
                not isinstance(key, str) or not key
                for key in record["leaf_snapshots"]
            )
        ):
            raise StopClosed("ledger collection shape is corrupt")
        if lock != {"device": 1, "inode": lock_inode}:
            raise StopClosed("arbitration lock identity changed")
        if (
            not cls.digest_is_valid(observed_sha256)
            or hashlib.sha256(cls.canonical(record)).hexdigest() != observed_sha256
        ):
            raise StopClosed("byte corruption")
        if record["authority"] != expected_authority:
            raise StopClosed("live authority changed")
        generation = record["generation"]
        previous = record["previous_sha256"]
        if (
            not isinstance(generation, int) or isinstance(generation, bool)
            or generation < 0 or (generation == 0 and previous is not None)
            or (generation > 0 and (
                not cls.digest_is_valid(previous)
            ))
            or (
                expected_previous is not None
                and (not cls.digest_is_valid(expected_previous)
                     or previous != expected_previous)
            )
        ):
            raise StopClosed("generation lineage is corrupt")
        common_keys = {
            "phase", "node", "prior", "created_at", "plan_observation",
            "arm_observation", "retry_observation",
        }

        def valid_observation(value: object) -> bool:
            return isinstance(value, dict) and (
                set(value) == {
                    "generation", "stream", "result_stream", "complete", "count",
                    "pages", "nodes", "body_bytes", "terminal_cursor", "completed_at",
                }
                and isinstance(value["generation"], int)
                and not isinstance(value["generation"], bool)
                and value["generation"] >= 0
                and cls.digest_is_valid(value["stream"])
                and cls.digest_is_valid(value["result_stream"])
                and type(value["complete"]) is bool
                and isinstance(value["count"], int)
                and not isinstance(value["count"], bool)
                and value["count"] >= 0
                and type(value["pages"]) is int and 1 <= value["pages"] <= 100
                and type(value["nodes"]) is int and 0 <= value["nodes"] <= 10_000
                and type(value["body_bytes"]) is int
                and 0 <= value["body_bytes"] <= 16 * 1024 * 1024
                and (
                    (value["nodes"] == 0 and value["terminal_cursor"] is None)
                    or (value["nodes"] > 0 and isinstance(value["terminal_cursor"], str)
                        and bool(value["terminal_cursor"]))
                )
                and cls.valid_timestamp(value["completed_at"])
            )

        observations = record.get("observation")
        if not isinstance(observations, dict) or set(observations) != {
            "comment", "child", "parent"
        }:
            raise StopClosed("observation container is corrupt")
        for observation in observations.values():
            if observation is not None and (
                not valid_observation(observation)
                or observation["generation"] > generation
            ):
                raise StopClosed("observation evidence is corrupt")

        for name in ("comment", "create", "link"):
            slot = record[name]
            if not isinstance(slot, dict) or slot.get("phase") not in {
                "none", "planned", "in-flight", "bound"
            }:
                raise StopClosed("slot phase is corrupt")
            expected_keys = (
                common_keys | {"parent", "child", "proof"}
                if name == "link" and slot["phase"] == "bound"
                else common_keys
            )
            if set(slot) != expected_keys:
                raise StopClosed("slot keys are corrupt")
            if slot["phase"] == "none" and (
                slot.get("node") is not None or slot.get("prior") is not None
                or slot.get("created_at") is not None
                or slot["plan_observation"] is not None
                or slot["arm_observation"] is not None
                or slot["retry_observation"] is not None
            ):
                raise StopClosed("none phase contains result state")
            if slot["phase"] == "planned" and (
                not valid_observation(slot["plan_observation"])
                or slot["arm_observation"] is not None
                or slot["retry_observation"] is not None
            ):
                raise StopClosed("planned phase lacks observation epoch")
            if slot["phase"] == "in-flight" and not (
                valid_observation(slot["plan_observation"])
                and valid_observation(slot["arm_observation"])
            ):
                raise StopClosed("in-flight phase lacks both observation epochs")
            if slot["phase"] == "in-flight" and (
                slot["retry_observation"] is not None
                and not (
                    name == "comment" and slot["node"] is not None
                    and valid_observation(slot["retry_observation"])
                    and slot["arm_observation"]["generation"]
                    < slot["retry_observation"]["generation"] < generation
                    and slot["retry_observation"]["complete"]
                    and slot["retry_observation"]["count"] == 1
                    and slot["retry_observation"]["result_stream"]
                    == cls.result_stream([{
                        "node": slot["node"], "author": "actor",
                        "body": "old-body", "marker": "program-marker",
                    }])
                    and valid_observation(observations["comment"])
                    and observations["comment"]["generation"]
                    >= slot["retry_observation"]["generation"]
                    and (
                        observations["comment"]["generation"]
                        != slot["retry_observation"]["generation"]
                        or observations["comment"] == slot["retry_observation"]
                    )
                )
            ):
                raise StopClosed("PATCH retry observation is corrupt")
            kind = {"comment": "comment", "create": "child", "link": "parent"}[name]
            current_observation = record["observation"][kind]
            expected_count = (
                1 if name == "comment" and slot["node"] is not None else 0
            )
            if slot["phase"] == "planned" and (
                slot["plan_observation"]["generation"] >= generation
                or not slot["plan_observation"]["complete"]
                or slot["plan_observation"]["count"] != expected_count
                or not valid_observation(current_observation)
                or current_observation["generation"]
                < slot["plan_observation"]["generation"]
            ):
                raise StopClosed("planned observation epochs are unreachable")
            if slot["phase"] == "in-flight":
                planned = slot["plan_observation"]
                armed = slot["arm_observation"]
                if (
                    not planned["complete"] or not armed["complete"]
                    or not planned["generation"] < armed["generation"] < generation
                    or planned["stream"] != armed["stream"]
                    or planned["result_stream"] != armed["result_stream"]
                    or planned["count"] != armed["count"]
                    or armed["count"] != expected_count
                    or not valid_observation(current_observation)
                    or current_observation["generation"] < armed["generation"]
                    or (
                        current_observation["generation"] == armed["generation"]
                        and current_observation != armed
                    )
                ):
                    raise StopClosed("in-flight observation epochs are unreachable")
            if slot["phase"] == "bound" and any(
                slot[field] is not None for field in (
                    "plan_observation", "arm_observation", "retry_observation"
                )
            ):
                raise StopClosed("bound phase retains ephemeral authority")
            if name == "comment" and slot["phase"] == "bound" and (
                not isinstance(slot["node"], str) or not slot["node"]
                or slot["prior"] is not None
                or slot["created_at"] is not None
            ):
                raise StopClosed("bound result is incomplete")
            if name == "create" and slot["phase"] == "bound" and (
                not isinstance(slot["node"], str) or not slot["node"]
                or slot["prior"] is not None
                or not cls.valid_timestamp(slot["created_at"])
            ):
                raise StopClosed("bound child result is incomplete")
            if name == "create" and slot["phase"] in {"planned", "in-flight"} and (
                slot["node"] is not None or slot["prior"] is not None
                or slot["created_at"] is not None
            ):
                raise StopClosed("create intent contains a result")
            if name == "link" and slot["phase"] in {"planned", "in-flight"} and (
                slot["node"] is not None or slot["prior"] is not None
                or slot["created_at"] is not None
            ):
                raise StopClosed("link intent contains a result")
            if name == "comment" and slot["phase"] in {"planned", "in-flight"} and not (
                slot["created_at"] is None and (
                    (slot["node"] is None and slot["prior"] is None)
                    or (isinstance(slot["node"], str) and bool(slot["node"])
                        and slot["prior"] == "old-body")
                )
            ):
                raise StopClosed("comment POST/PATCH intent is corrupt")
            if name == "link" and slot["phase"] == "bound" and (
                slot["node"] is not None or slot["prior"] is not None
                or slot["created_at"] is not None
                or slot["parent"] != "master" or slot["child"] != "issue-node"
                or not cls.digest_is_valid(slot["proof"])
            ):
                raise StopClosed("bound link proof is incomplete")
        if record["link"]["phase"] != "none" and not (
            record["create"]["phase"] == "bound"
            and record["create"]["node"] == "issue-node"
        ):
            raise StopClosed("link exists without its bound child")
        if record["next_graph"] is not None and not (
            record["comment"]["phase"] in {"planned", "in-flight"}
            and record["comment"]["node"] is not None
            and record["comment"]["prior"] == "old-body"
        ):
            raise StopClosed("next graph lacks its PATCH intent")
        if (
            any(
                not isinstance(snapshot, list) or len(snapshot) != 2
                or type(snapshot[0]) is not int or snapshot[0] < 0
                or not cls.digest_is_valid(snapshot[1])
                for snapshot in record["leaf_snapshots"].values()
            )
            or not set(record["leaf_snapshots"]).issubset(set(record["graph"]))
        ):
            raise StopClosed("leaf snapshot is outside the graph")
        model = cls()
        model.record = copy.deepcopy(record)
        model.lock_inode = lock_inode
        model.path_lock_inode = lock_inode
        model.report_present = report_present
        if remote_calls is not None:
            model.remote_calls = remote_calls
        return model

    def persist(
        self, mutate: object, expected_generation: int | None = None,
        expected_digest: str | None = None, expected_lock_inode: int = 41,
    ) -> None:
        generation = int(self.record["generation"])
        old_digest = self.digest()
        expected_authority = copy.deepcopy(self.record["authority"])
        if expected_generation is not None and (
            type(expected_generation) is not int
            or not 0 <= expected_generation <= self.MAX_INTEGER
            or expected_generation != generation
        ):
            raise StopClosed("stale generation")
        if expected_digest is not None and (
            not self.digest_is_valid(expected_digest) or expected_digest != old_digest
        ):
            raise StopClosed("stale digest")
        if expected_lock_inode != self.lock_inode or self.path_lock_inode != self.lock_inode:
            raise StopClosed("substituted lock")
        if self.record["lock"] != {"device": 1, "inode": expected_lock_inode}:
            raise StopClosed("persisted lock identity changed")
        candidate = copy.deepcopy(self.record)
        mutate(candidate)
        candidate["previous_sha256"] = old_digest
        candidate["generation"] = generation + 1
        candidate["self_inode"] = int(candidate["self_inode"]) + 1
        candidate_bytes = self.canonical(candidate)
        type(self).resume(
            candidate, self.lock_inode, candidate["self_inode"],
            hashlib.sha256(candidate_bytes).hexdigest(), expected_authority,
            expected_previous=old_digest,
        )
        self.record = candidate

    def replace_lock_path(self, inode: int) -> None:
        self.path_lock_inode = inode

    @staticmethod
    def stable_scan(
        cursors: list[str], node_ids: list[str], first_digest: str,
        second_digest: str, complete: bool = True, pages: int | None = None,
        nodes: int | None = None, body_bytes: int | None = None,
        timed_out: bool = False,
        body_sizes: list[int] | None = None,
    ) -> ScanPermit:
        body_sizes = [] if body_sizes is None else body_sizes
        if (
            not isinstance(cursors, list) or not isinstance(node_ids, list)
            or not isinstance(body_sizes, list)
            or any(not isinstance(value, str) for value in cursors + node_ids)
            or any(
                type(size) is not int or size < 0 or size > 65_536
                for size in body_sizes
            )
            or not isinstance(first_digest, str) or not isinstance(second_digest, str)
        ):
            raise StopClosed("pagination evidence has the wrong type")
        pages = max(1, len(cursors)) if pages is None else pages
        nodes = len(node_ids) if nodes is None else nodes
        body_bytes = sum(body_sizes) if body_bytes is None else body_bytes
        if (
            type(complete) is not bool or type(timed_out) is not bool
            or not complete or timed_out
            or type(pages) is not int or type(nodes) is not int
            or type(body_bytes) is not int
            or pages < 0 or pages > 100
            or nodes < 0 or nodes > 10_000 or body_bytes < 0
            or body_bytes > 16 * 1024 * 1024
            or pages != max(1, len(cursors)) or len(cursors) > 100
            or nodes != len(node_ids) or len(node_ids) > 10_000
            or (bool(node_ids) != bool(cursors))
            or body_bytes != sum(body_sizes)
            or any(
                not isinstance(size, int) or isinstance(size, bool)
                or size < 0 or size > 65_536 for size in body_sizes
            )
            or len(cursors) != len(set(cursors))
            or len(node_ids) != len(set(node_ids))
            or first_digest != second_digest
        ):
            raise StopClosed("pagination is incomplete, excessive, or changed")
        stream = hashlib.sha256(first_digest.encode("utf-8")).hexdigest()
        return ScanPermit(stream, {
            "pages": pages, "nodes": nodes, "body_bytes": body_bytes,
            "terminal_cursor": cursors[-1] if node_ids else None,
            "completed_at": "2026-08-14T00:00:00Z", "complete": True,
        })

    @staticmethod
    def validate_marker(namespace: list[tuple[str, str]], actor: str, bound: bool) -> None:
        if not bound and namespace:
            raise StopClosed("fresh namespace is not empty")
        if bound and (len(namespace) != 1 or namespace[0][1] != actor):
            raise StopClosed("marker is missing, duplicate, or wrong-author")

    def _observe(
        self, kind: str, stream: str, result_stream: str, count: int,
        complete: bool, scan: object, body_sizes: list[int],
    ) -> None:
        if scan is self.AUTO_SCAN:
            node_ids = [
                f"node-{index}" for index in range(max(count, len(body_sizes)))
            ]
            pages = max(1, (len(node_ids) + 99) // 100)
            cursors = [f"cursor-{index}" for index in range(pages)] if node_ids else []
            if complete:
                scan = self.stable_scan(
                    cursors, node_ids, stream, stream, body_sizes=body_sizes
                )
            else:
                scan = ScanPermit(
                    hashlib.sha256(stream.encode("utf-8")).hexdigest(),
                    {
                        "pages": pages, "nodes": len(node_ids),
                        "body_bytes": sum(body_sizes),
                        "terminal_cursor": cursors[-1] if cursors else None,
                        "completed_at": "2026-08-14T00:00:00Z", "complete": False,
                    },
                )
        if (
            not isinstance(scan, ScanPermit) or scan.used
            or scan.stream != hashlib.sha256(stream.encode("utf-8")).hexdigest()
            or scan.seal != hashlib.sha256(
                json.dumps(
                    scan.evidence, sort_keys=True, separators=(",", ":")
                ).encode()
            ).hexdigest()
            or set(scan.evidence) != {
                "pages", "nodes", "body_bytes", "terminal_cursor", "completed_at",
                "complete",
            }
            or scan.evidence["complete"] is not complete
            or type(scan.evidence["nodes"]) is not int
            or scan.evidence["nodes"] < count
        ):
            raise StopClosed("observation lacks a matching bounded scan permit")
        generation = int(self.record["generation"]) + 1
        evidence = copy.deepcopy(scan.evidence)
        self.persist(
            lambda record: record["observation"].update({
                kind: {
                    "generation": generation,
                    "stream": scan.stream,
                    "result_stream": result_stream,
                    "count": count, **evidence,
                }
            })
        )
        scan.used = True

    def observe_comment(
        self, stream: str = "comment-stable", count: int | None = None,
        complete: bool = True,
        namespace: list[tuple[str, str, str, str]] | None = None,
        body: str = "old-body",
        scan: object = AUTO_SCAN,
    ) -> None:
        marker_required = (
            self.record["comment"]["phase"] == "bound"
            or self.record["comment"]["node"] is not None
            or bool(count)
        )
        if namespace is None:
            namespace = (
                [("comment-node", "actor", "program-marker", body)]
                if marker_required else []
            )
        expected = [("comment-node", "actor", "program-marker", body)]
        if (marker_required and namespace != expected) or (not marker_required and namespace):
            raise StopClosed("comment marker/node/author/body is not exact")
        results = [
            {"node": node, "author": author, "marker": marker, "body": text}
            for node, author, marker, text in namespace
        ]
        self._observe(
            "comment", stream, self.result_stream(results),
            int(marker_required) if count is None else count, complete, scan,
            [len(result["body"].encode("utf-8")) for result in results],
        )

    def classify_child(
        self, issues: list[dict[str, object]], first_digest: str = "stable",
        second_digest: str = "stable", scan: object = AUTO_SCAN,
    ) -> tuple[list[dict[str, object]], list[dict[str, object]]]:
        if first_digest != second_digest:
            raise StopClosed("duplicate-search streams changed")
        candidates, evidence = [], []
        for issue in issues:
            marker = issue.get("marker")
            other_match = any(issue.get(field) is True for field in (
                "title", "body", "backlink", "parent"
            ))
            if marker == "matching" or issue.get("child_marker") == "program-child-marker" or other_match:
                candidates.append(issue)
            else:
                evidence.append(issue)
        self._observe(
            "child", first_digest, self.result_stream(candidates), len(candidates),
            True, scan, [0] * len(issues),
        )
        return candidates, evidence

    def observe_parent(
        self, relationships: list[str] | None = None, stream: str = "parent-stable",
        complete: bool = True, child_parent: str = "master",
        scan: object = AUTO_SCAN,
    ) -> None:
        relationships = relationships or []
        proof = [{"child_parent": child_parent, "parent_subissues": relationships}]
        self._observe(
            "parent", stream, self.result_stream(proof), len(relationships), complete,
            scan, [],
        )

    def _plan(self, slot: str, node: str | None = None, prior: str | None = None) -> None:
        current = self.record[slot]
        if current["phase"] != "none" and not (
            slot == "comment" and current["phase"] == "bound"
        ):
            raise StopClosed("slot already owns an intent")
        kind = {"comment": "comment", "create": "child", "link": "parent"}[slot]
        observation = copy.deepcopy(self.record["observation"][kind])
        self.persist(lambda r: r[slot].update(
            phase="planned", node=node, prior=prior,
            plan_observation=observation, arm_observation=None,
            retry_observation=None,
        ))

    def plan_comment(self) -> None:
        observation = self.record["observation"]["comment"]
        phase = self.record["comment"]["phase"]
        expected_count = 1 if phase == "bound" else 0
        if (
            not observation or not observation["complete"]
            or observation["count"] != expected_count
        ):
            raise StopClosed("comment scan evidence is absent")
        self._plan(
            "comment",
            node=self.record["comment"]["node"] if phase == "bound" else None,
            prior="old-body" if phase == "bound" else None,
        )

    def plan_create(self) -> None:
        observation = self.record["observation"]["child"]
        if not observation or not observation["complete"] or observation["count"]:
            raise StopClosed("child is not proven missing")
        self._plan("create")

    def plan_link(self) -> None:
        create = self.record["create"]
        if create["phase"] != "bound" or create["node"] != "issue-node":
            raise StopClosed("link lacks the exact bound child")
        observation = self.record["observation"]["parent"]
        if not observation or not observation["complete"] or observation["count"]:
            raise StopClosed("parent scan evidence is absent")
        self._plan("link")

    def arm(self, slot: str) -> CallPermit:
        if self.record[slot]["phase"] != "planned":
            raise StopClosed("remote call lacks planned intent")
        kind = {"comment": "comment", "create": "child", "link": "parent"}[slot]
        current = self.record["observation"][kind]
        planned = self.record[slot]["plan_observation"]
        if (
            not planned["complete"] or not current or not current["complete"]
            or current["generation"] <= planned["generation"]
        ):
            raise StopClosed("persisted observation evidence changed")
        expected_count = (
            1 if slot == "comment" and self.record[slot]["node"] is not None else 0
        )
        if (
            current["stream"] != planned["stream"]
            or current["result_stream"] != planned["result_stream"]
            or current["count"] != planned["count"]
        ):
            self.persist(
                lambda record: record[slot].update(
                    plan_observation=copy.deepcopy(current)
                )
            )
            raise StopClosed("consecutive observation evidence changed")
        if current["count"] != expected_count:
            raise StopClosed("mutation prerequisite no longer holds")
        self.persist(lambda r: r[slot].update(
            phase="in-flight", arm_observation=copy.deepcopy(current)
        ))
        return CallPermit(slot, int(self.record["generation"]))

    def execute(self, permit: CallPermit) -> None:
        if (
            permit.used or permit.generation != self.record["generation"]
            or self.record[permit.slot]["phase"] != "in-flight"
            or self.path_lock_inode != self.lock_inode
        ):
            raise StopClosed("call permit is absent, stale, or already used")
        permit.used = True
        self.remote_calls[permit.slot] += 1

    def _bind(
        self, slot: str, exact_results: list[str], result_stream: str,
        created_at: str | None = None,
    ) -> None:
        kind = {"comment": "comment", "create": "child"}[slot]
        observed = self.record["observation"][kind]
        armed = self.record[slot]["arm_observation"]
        if (
            self.record[slot]["phase"] != "in-flight" or len(exact_results) != 1
            or not observed or not observed["complete"]
            or observed["generation"] <= armed["generation"]
            or observed["count"] != 1
            or observed["result_stream"] != result_stream
        ):
            raise StopClosed("remote result is uncertain")
        self.persist(
            lambda r: r[slot].update(
                phase="bound", node=exact_results[0], prior=None,
                created_at=created_at,
                plan_observation=None, arm_observation=None, retry_observation=None,
            )
        )

    def bind_comment(self, results: list[dict[str, str]]) -> None:
        if len(results) != 1:
            raise StopClosed("comment namespace contains conflicting results")
        exact = [
            result for result in results
            if result == {
                "node": "comment-node", "author": "actor",
                "body": "intended-body", "marker": "program-marker",
            }
        ]
        self._bind(
            "comment", [result["node"] for result in exact],
            self.result_stream(results),
        )

    def bind_create(self, candidates: list[dict[str, str]]) -> None:
        if len(candidates) != 1:
            raise StopClosed("child search contains conflicting candidates")
        exact = [
            item for item in candidates
            if set(item) == {
                "node", "creator", "title", "body", "child_marker", "created_at"
            }
            and item["node"] == "issue-node" and item["creator"] == "actor"
            and item["title"] == "title" and item["body"] == "body"
            and item["child_marker"] == "program-child-marker"
            and self.valid_timestamp(item["created_at"])
        ]
        self._bind(
            "create", [item["node"] for item in exact],
            self.result_stream(candidates),
            exact[0]["created_at"] if exact else None,
        )

    def bind_link(self, child_parent: str, parent_subissues: list[str]) -> None:
        if child_parent != "master" or parent_subissues.count("issue-node") != 1:
            raise StopClosed("parent-child pair is not proven in both directions")
        observed = self.record["observation"]["parent"]
        armed = self.record["link"]["arm_observation"]
        proof = [{"child_parent": child_parent, "parent_subissues": parent_subissues}]
        proof_digest = self.result_stream(proof)
        if (
            self.record["link"]["phase"] != "in-flight" or not observed
            or not observed["complete"] or observed["generation"] <= armed["generation"]
            or observed["count"] != 1
            or proof_digest != observed["result_stream"]
        ):
            raise StopClosed("link result lacks durable intent")
        self.persist(
            lambda record: record["link"].update(
                phase="bound", node=None, prior=None, parent="master",
                child="issue-node", proof=observed["stream"],
                plan_observation=None, arm_observation=None, retry_observation=None,
            )
        )

    def rekey(self, authority: list[str], next_graph: list[str], node: str) -> None:
        if (
            authority != self.record["authority"]
            or self.record["comment"]["phase"] != "bound"
            or self.record["comment"]["node"] != node
        ):
            raise StopClosed("rekey changes authority or comment node")
        observation = self.record["observation"]["comment"]
        if not observation or not observation["complete"] or observation["count"] != 1:
            raise StopClosed("rekey lacks complete comment observation")
        self.persist(lambda record: (
            record.update(next_graph=copy.deepcopy(next_graph)),
            record["comment"].update(
                phase="planned", node=node, prior="old-body",
                plan_observation=copy.deepcopy(observation),
                arm_observation=None, retry_observation=None,
            ),
        ))

    def finish_patch(self, remote_body: str, result_node: str) -> CallPermit | None:
        if (
            self.record["comment"]["phase"] != "in-flight"
            or result_node != self.record["comment"]["node"]
        ):
            raise StopClosed("PATCH result or prior body drifted")
        observed = self.record["observation"]["comment"]
        armed = self.record["comment"]["arm_observation"]
        if (
            not observed or not observed["complete"]
            or observed["generation"] <= armed["generation"]
            or observed["count"] != 1
        ):
            raise StopClosed("PATCH result lacks complete post-call observation")
        result = [{
            "node": result_node, "author": "actor", "body": remote_body,
            "marker": "program-marker",
        }]
        if observed["result_stream"] != self.result_stream(result):
            raise StopClosed("PATCH claim differs from observed result")
        if remote_body == "old-body":
            recovery = self.record["comment"]["retry_observation"]
            if recovery is None:
                self.persist(
                    lambda record: record["comment"].update(
                        retry_observation=copy.deepcopy(observed)
                    )
                )
                raise StopClosed("PATCH retry needs a recovery baseline")
            if (
                observed["generation"] <= recovery["generation"]
                or observed["stream"] != recovery["stream"]
                or observed["result_stream"] != recovery["result_stream"]
                or observed["count"] != recovery["count"]
            ):
                self.persist(
                    lambda record: record["comment"].update(
                        retry_observation=copy.deepcopy(observed)
                    )
                )
                raise StopClosed("PATCH retry needs a second identical observation")
            return CallPermit("comment", int(self.record["generation"]))
        if remote_body != "intended-body":
            raise StopClosed("PATCH result or prior body drifted")
        def bind(record: dict[str, object]) -> None:
            record["comment"].update(
                phase="bound", prior=None,
                plan_observation=None, arm_observation=None,
                retry_observation=None,
            )
            if record["next_graph"] is not None:
                record["graph"] = copy.deepcopy(record["next_graph"])
                record["next_graph"] = None

        self.persist(bind)
        return None

    def compose_leaf(self, position: str, generation: int, digest: str) -> None:
        snapshot = self.record["leaf_snapshots"].get(position)
        if (
            not isinstance(position, str) or not position
            or type(generation) is not int or generation < 0
            or not self.digest_is_valid(digest)
            or snapshot != [generation, digest]
            or position not in self.record["graph"]
        ):
            raise StopClosed("leaf ownership or evidence does not match graph")


class ProgramLedgerModelTests(unittest.TestCase):
    @staticmethod
    def exact_comment() -> dict[str, str]:
        return {
            "node": "comment-node", "author": "actor",
            "body": "intended-body", "marker": "program-marker",
        }

    @staticmethod
    def exact_child() -> dict[str, str]:
        return {
            "node": "issue-node", "creator": "actor", "title": "title",
            "body": "body", "child_marker": "program-child-marker",
            "created_at": "2026-08-14T00:00:00.123Z",
        }

    @staticmethod
    def refresh_before_arm(model: ProgramLedgerModel, slot: str) -> None:
        if slot == "comment":
            model.observe_comment(count=int(model.record["comment"]["node"] is not None))
        elif slot == "create":
            model.classify_child([])
        else:
            model.observe_parent()

    def observe_result(self, model: ProgramLedgerModel, slot: str) -> None:
        if slot == "comment":
            results = [self.exact_comment()]
            model.observe_comment(
                stream="full-comment-stream-with-unrelated", count=1,
                body="intended-body",
            )
        elif slot == "create":
            results = [self.exact_child()]
            model.classify_child(
                results, "full-child-stream-with-unrelated",
                "full-child-stream-with-unrelated",
            )
        else:
            model.observe_parent(["issue-node"], stream="parent-post")

    def test_fresh_and_persisted_resume_are_distinct(self) -> None:
        model = ProgramLedgerModel.fresh(False, False)
        with self.assertRaises(StopClosed):
            ProgramLedgerModel.fresh(True, False)
        resumed = ProgramLedgerModel.resume(
            model.record, model.lock_inode, model.record["self_inode"],
            model.digest(), model.record["authority"],
        )
        self.assertEqual(resumed.record, model.record)
        for args in (
            (None, 41, 101, None, model.record["authority"]),
            (model.record, None, 101, model.digest(), model.record["authority"]),
        ):
            with self.subTest(args=args):
                with self.assertRaises(StopClosed):
                    ProgramLedgerModel.resume(*args)

    def test_corrupt_lost_or_inode_substituted_ledger_stops(self) -> None:
        model = ProgramLedgerModel()
        with self.assertRaises(StopClosed):
            ProgramLedgerModel.resume(7, 41, 101, "0" * 64, model.record["authority"])
        corrupt = copy.deepcopy(model.record)
        corrupt["unknown"] = True
        bad_phase = copy.deepcopy(model.record)
        bad_phase["comment"]["phase"] = "garbage"
        bad_authority = copy.deepcopy(model.record)
        bad_authority["authority"] = ["other"] * 4
        bad_bound = copy.deepcopy(model.record)
        bad_bound["comment"]["phase"] = "bound"
        bad_next = copy.deepcopy(model.record)
        bad_next["next_graph"] = ["leaf-2"]
        bad_link = copy.deepcopy(model.record)
        bad_link["create"].update(phase="bound", node="issue-node")
        bad_link["link"].update(phase="planned", node="result", prior="unexpected")
        bad_graph = copy.deepcopy(model.record)
        bad_graph["graph"] = None
        bad_snapshots = copy.deepcopy(model.record)
        bad_snapshots["leaf_snapshots"] = None
        cases = (
            (corrupt, 41, 101, model.digest(), model.record["authority"]),
            (model.record, 41, 999, model.digest(), model.record["authority"]),
            (model.record, 41, 101, "0" * 64, model.record["authority"]),
            (model.record, 99, 101, model.digest(), model.record["authority"]),
            (bad_phase, 41, 101,
             hashlib.sha256(ProgramLedgerModel.canonical(bad_phase)).hexdigest(),
             model.record["authority"]),
            (bad_authority, 41, 101,
             hashlib.sha256(ProgramLedgerModel.canonical(bad_authority)).hexdigest(),
             model.record["authority"]),
            (bad_bound, 41, 101,
             hashlib.sha256(ProgramLedgerModel.canonical(bad_bound)).hexdigest(),
             model.record["authority"]),
            (bad_next, 41, 101,
             hashlib.sha256(ProgramLedgerModel.canonical(bad_next)).hexdigest(),
             model.record["authority"]),
            (bad_link, 41, 101,
             hashlib.sha256(ProgramLedgerModel.canonical(bad_link)).hexdigest(),
             model.record["authority"]),
            (bad_graph, 41, 101,
             hashlib.sha256(ProgramLedgerModel.canonical(bad_graph)).hexdigest(),
             model.record["authority"]),
            (bad_snapshots, 41, 101,
             hashlib.sha256(ProgramLedgerModel.canonical(bad_snapshots)).hexdigest(),
             model.record["authority"]),
        )
        for args in cases:
            with self.subTest(args=args):
                with self.assertRaises(StopClosed):
                    ProgramLedgerModel.resume(*args)
        retry = ProgramLedgerModel()
        retry.observe_comment()
        retry.plan_comment()
        retry.record["comment"]["retry_observation"] = {"malformed": True}
        with self.assertRaises(StopClosed):
            ProgramLedgerModel.resume(
                retry.record, 41, retry.record["self_inode"], retry.digest(),
                retry.record["authority"],
            )
        malformed_observations: tuple[object, ...] = (
            None, 7, {}, {"comment": None},
            {"comment": None, "child": None, "parent": None, "extra": None},
            {"comment": "wrong", "child": None, "parent": None},
        )
        for observation in malformed_observations:
            corrupt_observation = copy.deepcopy(model.record)
            corrupt_observation["observation"] = observation
            with self.subTest(observation=observation), self.assertRaises(StopClosed):
                ProgramLedgerModel.resume(
                    corrupt_observation, 41, corrupt_observation["self_inode"],
                    hashlib.sha256(
                        ProgramLedgerModel.canonical(corrupt_observation)
                    ).hexdigest(),
                    corrupt_observation["authority"],
                )
        inode_cases: list[tuple[dict[str, object], object, object]] = []
        for value in (True, 0):
            corrupt_inode = copy.deepcopy(model.record)
            corrupt_inode["self_inode"] = value
            inode_cases.append((corrupt_inode, 41, value))
        corrupt_device = copy.deepcopy(model.record)
        corrupt_device["lock"]["device"] = True
        inode_cases.append((corrupt_device, 41, 101))
        for value in (True, 0):
            corrupt_lock = copy.deepcopy(model.record)
            corrupt_lock["lock"]["inode"] = value
            inode_cases.append((corrupt_lock, value, 101))
        for corrupt_inode, live_lock, live_inode in inode_cases:
            with self.subTest(
                self_inode=corrupt_inode["self_inode"], lock=corrupt_inode["lock"]
            ), self.assertRaises(StopClosed):
                ProgramLedgerModel.resume(
                    corrupt_inode, live_lock, live_inode,
                    hashlib.sha256(
                        ProgramLedgerModel.canonical(corrupt_inode)
                    ).hexdigest(),
                    corrupt_inode["authority"],
                )
        lineage = ProgramLedgerModel()
        lineage.persist(lambda record: None)
        for digest in ("g" * 64, "A" * 64, "0" * 63 + "!", "0" * 63, "0" * 65):
            corrupt_lineage = copy.deepcopy(lineage.record)
            corrupt_lineage["previous_sha256"] = digest
            with self.subTest(lineage=digest), self.assertRaises(StopClosed):
                ProgramLedgerModel.resume(
                    corrupt_lineage, 41, corrupt_lineage["self_inode"],
                    hashlib.sha256(
                        ProgramLedgerModel.canonical(corrupt_lineage)
                    ).hexdigest(),
                    corrupt_lineage["authority"],
                )
        observed = ProgramLedgerModel()
        observed.observe_comment()
        for field in ("stream", "result_stream"):
            corrupt_observation = copy.deepcopy(observed.record)
            corrupt_observation["observation"]["comment"][field] = "z" * 64
            with self.subTest(observation_digest=field), self.assertRaises(StopClosed):
                ProgramLedgerModel.resume(
                    corrupt_observation, 41, corrupt_observation["self_inode"],
                    hashlib.sha256(
                        ProgramLedgerModel.canonical(corrupt_observation)
                    ).hexdigest(),
                    corrupt_observation["authority"],
                )
        corrupt_snapshot = copy.deepcopy(model.record)
        corrupt_snapshot["leaf_snapshots"]["leaf-1"][1] = "z" * 64
        with self.assertRaises(StopClosed):
            ProgramLedgerModel.resume(
                corrupt_snapshot, 41, corrupt_snapshot["self_inode"],
                hashlib.sha256(
                    ProgramLedgerModel.canonical(corrupt_snapshot)
                ).hexdigest(),
                corrupt_snapshot["authority"],
            )

    def test_bound_slots_reject_retained_ephemeral_authority(self) -> None:
        models: list[tuple[ProgramLedgerModel, str]] = []

        comment = ProgramLedgerModel()
        comment.observe_comment()
        comment.plan_comment()
        self.refresh_before_arm(comment, "comment")
        comment.execute(comment.arm("comment"))
        self.observe_result(comment, "comment")
        comment.bind_comment([self.exact_comment()])
        models.append((comment, "comment"))

        child = ProgramLedgerModel()
        child.classify_child([])
        child.plan_create()
        self.refresh_before_arm(child, "create")
        child.execute(child.arm("create"))
        self.observe_result(child, "create")
        child.bind_create([self.exact_child()])
        models.append((child, "create"))

        link = copy.deepcopy(child)
        link.observe_parent()
        link.plan_link()
        self.refresh_before_arm(link, "link")
        link.execute(link.arm("link"))
        self.observe_result(link, "link")
        link.bind_link("master", ["issue-node"])
        models.append((link, "link"))

        for model, slot in models:
            corrupt = copy.deepcopy(model.record)
            evidence = copy.deepcopy(corrupt["observation"][{
                "comment": "comment", "create": "child", "link": "parent"
            }[slot]])
            corrupt[slot]["plan_observation"] = evidence
            corrupt[slot]["arm_observation"] = evidence
            with self.subTest(slot=slot), self.assertRaises(StopClosed):
                ProgramLedgerModel.resume(
                    corrupt, 41, corrupt["self_inode"],
                    hashlib.sha256(ProgramLedgerModel.canonical(corrupt)).hexdigest(),
                    corrupt["authority"],
                )
            if slot in {"comment", "create"}:
                empty_node = copy.deepcopy(model.record)
                empty_node[slot]["node"] = ""
                with self.subTest(slot=slot, defect="empty-node"), self.assertRaises(
                    StopClosed
                ):
                    ProgramLedgerModel.resume(
                        empty_node, 41, empty_node["self_inode"],
                        hashlib.sha256(
                            ProgramLedgerModel.canonical(empty_node)
                        ).hexdigest(),
                        empty_node["authority"],
                    )
        patch_intent = copy.deepcopy(comment)
        patch_intent.observe_comment()
        patch_intent.plan_comment()
        patch_intent.record["comment"]["node"] = ""
        with self.assertRaises(StopClosed):
            ProgramLedgerModel.resume(
                patch_intent.record, 41, patch_intent.record["self_inode"],
                patch_intent.digest(), patch_intent.record["authority"],
            )
        corrupt_proof = copy.deepcopy(link.record)
        corrupt_proof["link"]["proof"] = "z" * 64
        with self.assertRaises(StopClosed):
            ProgramLedgerModel.resume(
                corrupt_proof, 41, corrupt_proof["self_inode"],
                hashlib.sha256(
                    ProgramLedgerModel.canonical(corrupt_proof)
                ).hexdigest(),
                corrupt_proof["authority"],
            )

    def test_filesystem_lock_is_stable_across_atomic_ledger_replace(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            lock = root / "ledger.lock"
            ledger = root / "ledger.json"
            flags = os.O_CREAT | os.O_EXCL | os.O_RDWR | os.O_NOFOLLOW
            lock_fd = os.open(lock, flags, 0o600)
            self.addCleanup(os.close, lock_fd)
            with self.assertRaises(FileExistsError):
                os.open(lock, flags, 0o600)
            fcntl.flock(lock_fd, fcntl.LOCK_EX | fcntl.LOCK_NB)
            self.assertEqual(os.fstat(lock_fd).st_mode & 0o777, 0o600)
            competing_fd = os.open(lock, os.O_RDWR | os.O_NOFOLLOW)
            self.addCleanup(os.close, competing_fd)
            with self.assertRaises(BlockingIOError):
                fcntl.flock(competing_fd, fcntl.LOCK_EX | fcntl.LOCK_NB)
            lock_inode = os.fstat(lock_fd).st_ino
            ledger_fd = os.open(ledger, flags, 0o600)
            os.write(ledger_fd, b"old")
            os.fsync(ledger_fd)
            old_inode = os.fstat(ledger_fd).st_ino
            os.close(ledger_fd)
            temporary = root / "ledger.tmp"
            temporary_fd = os.open(temporary, flags, 0o600)
            os.write(temporary_fd, b"new")
            os.fsync(temporary_fd)
            new_inode = os.fstat(temporary_fd).st_ino
            os.close(temporary_fd)
            self.assertNotEqual(old_inode, new_inode)
            with self.assertRaises(FileExistsError):
                os.open(temporary, flags, 0o600)
            os.replace(temporary, ledger)
            directory_fd = os.open(root, os.O_RDONLY | os.O_DIRECTORY)
            os.fsync(directory_fd)
            os.close(directory_fd)
            self.assertEqual(os.stat(lock).st_ino, lock_inode)
            self.assertEqual(os.stat(ledger).st_ino, new_inode)
            replacement = root / "replacement.lock"
            replacement_fd = os.open(replacement, flags, 0o600)
            os.close(replacement_fd)
            os.replace(replacement, lock)
            self.assertNotEqual(os.stat(lock).st_ino, lock_inode)
            model = ProgramLedgerModel()
            model.replace_lock_path(os.stat(lock).st_ino)
            with self.assertRaises(StopClosed):
                model.persist(lambda record: None)
            model = ProgramLedgerModel()
            model.observe_comment()
            model.plan_comment()
            self.refresh_before_arm(model, "comment")
            permit = model.arm("comment")
            model.replace_lock_path(os.stat(lock).st_ino)
            with self.assertRaises(StopClosed):
                model.execute(permit)

    def test_coordinate_digest_is_injective_for_hyphen_name_collision(self) -> None:
        first = ProgramLedgerModel.coordinate("R_foo-bar_baz", "I_1")
        second = ProgramLedgerModel.coordinate("R_foo_bar-baz", "I_2")
        self.assertEqual(
            first,
            "ba30abf23ca48a0aeec077633e1fdfed9aca308ed41295faed046198085fa4ee",
        )
        self.assertNotEqual(first, second)
        self.assertEqual(
            ProgramLedgerModel.coordinate("R_é", "I_ß"),
            "e45d2af641903f03a0a4acbe30aa15a1929f0b928861a4ece834f4d8a53d7355",
        )
        self.assertEqual(
            ProgramLedgerModel.result_stream([{"body": "é"}]),
            "54cc82f0270ab5b28544fd482dee4783da86df0c27133ac870ddf8bb5473f2c4",
        )
        with self.assertRaises(StopClosed):
            ProgramLedgerModel.coordinate("R_e\N{COMBINING ACUTE ACCENT}", "I_1")
        with self.assertRaises(StopClosed):
            ProgramLedgerModel.result_stream([{"body": "e\N{COMBINING ACUTE ACCENT}"}])

    def test_multi_page_bounds_and_stream_stability(self) -> None:
        ProgramLedgerModel.stable_scan(["a", "b"], ["1", "2"], "d", "d")
        ProgramLedgerModel.stable_scan(
            [], [], "d", "d", body_bytes=65_536, body_sizes=[65_536]
        )
        cases = (
            {"complete": False}, {"timed_out": True}, {"pages": 101},
            {"nodes": 10_001}, {"body_bytes": 16 * 1024 * 1024 + 1},
            {"pages": -1}, {"nodes": -1}, {"body_bytes": -1},
            {"body_sizes": [65_537]}, {"body_sizes": [-1]},
            {"pages": True}, {"nodes": False}, {"body_bytes": True},
            {"complete": 1}, {"timed_out": 0},
        )
        for kwargs in cases:
            with self.subTest(kwargs=kwargs), self.assertRaises(StopClosed):
                ProgramLedgerModel.stable_scan([], [], "d", "d", **kwargs)
        with self.assertRaises(StopClosed):
            ProgramLedgerModel.stable_scan(["a", "a"], ["1"], "d", "d")
        with self.assertRaises(StopClosed):
            ProgramLedgerModel.stable_scan([], [], "before", "after")
        bypasses = (
            {"cursors": [], "node_ids": [], "body_sizes": [65_536] * 257,
             "body_bytes": 0},
            {"cursors": [], "node_ids": [str(index) for index in range(10_001)],
             "nodes": 0},
            {"cursors": [str(index) for index in range(101)], "node_ids": [],
             "pages": 0},
        )
        for bypass in bypasses:
            with self.subTest(bypass=tuple(bypass)), self.assertRaises(StopClosed):
                ProgramLedgerModel.stable_scan(
                    bypass.pop("cursors"), bypass.pop("node_ids"), "d", "d", **bypass
                )

    def test_mutations_require_consumed_bounded_scan_evidence(self) -> None:
        comment = ProgramLedgerModel()
        with self.assertRaises(StopClosed):
            comment.observe_comment(scan=None)
        permit = ProgramLedgerModel.stable_scan([], [], "comment-stable", "comment-stable")
        permit.evidence["pages"] = 0
        with self.assertRaises(StopClosed):
            comment.observe_comment(scan=permit)
        self.assertIsNone(comment.record["observation"]["comment"])

        comment.observe_comment()
        comment.plan_comment()
        with self.assertRaises(StopClosed):
            comment.observe_comment(scan=None)
        with self.assertRaises(StopClosed):
            comment.arm("comment")
        self.assertEqual(comment.remote_calls["comment"], 0)

        child = ProgramLedgerModel()
        with self.assertRaises(StopClosed):
            child.classify_child([], scan=None)
        child.classify_child([])
        child.plan_create()
        with self.assertRaises(StopClosed):
            child.classify_child([], scan=None)
        with self.assertRaises(StopClosed):
            child.arm("create")
        self.assertEqual(child.remote_calls["create"], 0)

    def test_ledger_size_and_nested_value_bounds_precede_parsing(self) -> None:
        maximum = ProgramLedgerModel.MAX_LEDGER_BYTES
        self.assertTrue(ProgramLedgerModel.raw_size_allowed(maximum))
        self.assertFalse(ProgramLedgerModel.raw_size_allowed(maximum + 1))
        self.assertEqual(ProgramLedgerModel.decode(b'{"x":"ok"}\n'), {"x": "ok"})
        with self.assertRaises(StopClosed):
            ProgramLedgerModel.decode(b" " * (maximum + 1))

        model = ProgramLedgerModel()
        corrupt = copy.deepcopy(model.record)
        corrupt["graph"] = ["x" * (ProgramLedgerModel.MAX_STRING_BYTES + 1)]
        with self.assertRaises(StopClosed):
            ProgramLedgerModel.resume(
                corrupt, 41, corrupt["self_inode"],
                hashlib.sha256(ProgramLedgerModel.canonical(corrupt)).hexdigest(),
                corrupt["authority"],
            )
        self.assertTrue(ProgramLedgerModel.value_is_bounded(
            {"ledger_path": "p" * 4_096, "items": [None] * 10_000}
        ))
        self.assertFalse(ProgramLedgerModel.value_is_bounded(
            {"ledger_path": "p" * 4_097}
        ))
        self.assertFalse(ProgramLedgerModel.value_is_bounded(
            {"items": [None] * 10_001}
        ))

    def test_canonical_bytes_nfc_duplicates_and_depth_fail_closed(self) -> None:
        self.assertEqual(
            ProgramLedgerModel.canonical({"x": "é"}), b'{"x":"\xc3\xa9"}\n'
        )
        self.assertEqual(ProgramLedgerModel.decode(b'{"x":"\xc3\xa9"}\n'), {"x": "é"})
        invalid = (
            b'{ "x" : "ok" }\n', b'{"x":"ok"}', b'\xef\xbb\xbf{"x":"ok"}\n',
            b'{"x":1,"x":2}\n', b'{"x":"\\u00e9"}\n',
            '{"x":"e\N{COMBINING ACUTE ACCENT}"}\n'.encode("utf-8"),
        )
        for raw in invalid:
            with self.subTest(raw=raw), self.assertRaises(StopClosed):
                ProgramLedgerModel.decode(raw)
        with self.assertRaises(StopClosed):
            ProgramLedgerModel.canonical({"x": "e\N{COMBINING ACUTE ACCENT}"})

        at_limit: object = 0
        for _ in range(14):
            at_limit = [at_limit]
        self.assertTrue(ProgramLedgerModel.value_is_bounded({"x": at_limit}))
        at_limit_bytes = ProgramLedgerModel.canonical({"x": at_limit})
        ProgramLedgerModel.decode(at_limit_bytes)
        over_limit = [at_limit]
        self.assertFalse(ProgramLedgerModel.value_is_bounded({"x": over_limit}))
        with self.assertRaises(StopClosed):
            ProgramLedgerModel.canonical({"x": over_limit})
        over_limit_bytes = (
            json.dumps({"x": over_limit}, separators=(",", ":")) + "\n"
        ).encode()
        with self.assertRaises(StopClosed):
            ProgramLedgerModel.decode(over_limit_bytes)

    def test_integer_and_surrogate_tokens_fail_closed(self) -> None:
        maximum = ProgramLedgerModel.MAX_INTEGER
        self.assertEqual(
            ProgramLedgerModel.decode(f'{{"x":{maximum}}}\n'.encode()),
            {"x": maximum},
        )
        invalid_integers = (
            f'{{"x":{maximum + 1}}}\n'.encode(),
            b'{"x":-' + str(maximum).encode() + b'}\n',
            b'{"x":' + (b"9" * 5_000) + b'}\n',
        )
        for raw in invalid_integers:
            with self.subTest(raw_length=len(raw)), self.assertRaises(StopClosed):
                ProgramLedgerModel.decode(raw)
        for raw in (
            b'{"x":"\\ud800"}\n', b'{"x":"\\udfff"}\n',
            b'{"\\ud800":1}\n', b'{"\\udfff":1}\n',
        ):
            with self.subTest(raw=raw), self.assertRaises(StopClosed):
                ProgramLedgerModel.decode(raw)
        for value in ({"x": "\ud800"}, {"\udfff": 1}):
            with self.subTest(value=value), self.assertRaises(StopClosed):
                ProgramLedgerModel.canonical(value)

    def test_canonical_size_is_atomic_at_persistence_boundary(self) -> None:
        model = ProgramLedgerModel()
        items = ["leaf-1"]
        items.extend(
            "x" * (ProgramLedgerModel.MAX_STRING_BYTES - 6) + f"{index:06d}"
            for index in range(127)
        )
        items.append("")
        candidate = copy.deepcopy(model.record)
        candidate["graph"] = items
        candidate["previous_sha256"] = model.digest()
        candidate["generation"] += 1
        candidate["self_inode"] += 1
        unbounded = (
            json.dumps(
                candidate, ensure_ascii=False, allow_nan=False, sort_keys=True,
                separators=(",", ":"),
            ) + "\n"
        ).encode()
        gap = ProgramLedgerModel.MAX_LEDGER_BYTES - len(unbounded)
        self.assertGreaterEqual(gap, 0)
        self.assertLessEqual(gap, ProgramLedgerModel.MAX_STRING_BYTES)
        items[-1] = "y" * gap
        model.persist(lambda record: record.update(graph=copy.deepcopy(items)))
        self.assertEqual(len(model.canonical(model.record)), model.MAX_LEDGER_BYTES)
        retained = copy.deepcopy(model.record)
        with self.assertRaises(StopClosed):
            model.persist(lambda record: record["graph"].__setitem__(-1, items[-1] + "x"))
        self.assertEqual(model.record, retained)

    def test_resume_rejects_unreachable_observation_epochs(self) -> None:
        def planned(slot: str) -> ProgramLedgerModel:
            model = ProgramLedgerModel()
            if slot == "comment":
                model.observe_comment()
                model.plan_comment()
            elif slot == "create":
                model.classify_child([])
                model.plan_create()
            else:
                model.record["create"].update(
                    phase="bound", node="issue-node",
                    created_at="2026-08-14T00:00:00Z",
                )
                model.observe_parent()
                model.plan_link()
            return model

        for slot in ("comment", "create", "link"):
            base = planned(slot)
            kind = {"comment": "comment", "create": "child", "link": "parent"}[slot]
            future = copy.deepcopy(base.record)
            future["observation"][kind]["generation"] = future["generation"] + 1
            with self.subTest(slot=slot, defect="future"), self.assertRaises(StopClosed):
                ProgramLedgerModel.resume(
                    future, 41, future["self_inode"],
                    hashlib.sha256(ProgramLedgerModel.canonical(future)).hexdigest(),
                    future["authority"],
                )

            self.refresh_before_arm(base, slot)
            base.arm(slot)
            for defect in ("equal", "future-arm", "reversed"):
                corrupt = copy.deepcopy(base.record)
                if defect == "equal":
                    corrupt[slot]["plan_observation"]["generation"] = corrupt[slot][
                        "arm_observation"
                    ]["generation"]
                elif defect == "future-arm":
                    corrupt[slot]["arm_observation"]["generation"] = (
                        corrupt["generation"]
                    )
                else:
                    corrupt[slot]["plan_observation"]["generation"] = corrupt[slot][
                        "arm_observation"
                    ]["generation"] + 1
                with self.subTest(slot=slot, defect=defect), self.assertRaises(StopClosed):
                    ProgramLedgerModel.resume(
                        corrupt, 41, corrupt["self_inode"],
                        hashlib.sha256(
                            ProgramLedgerModel.canonical(corrupt)
                        ).hexdigest(),
                        corrupt["authority"],
                    )

    def test_retry_epochs_and_prospective_semantics_fail_closed(self) -> None:
        patch = ProgramLedgerModel()
        patch.observe_comment()
        patch.plan_comment()
        self.refresh_before_arm(patch, "comment")
        patch.execute(patch.arm("comment"))
        self.observe_result(patch, "comment")
        patch.bind_comment([self.exact_comment()])
        patch.observe_comment()
        patch.rekey(patch.record["authority"], ["leaf-1", "leaf-2"], "comment-node")
        self.refresh_before_arm(patch, "comment")
        patch.execute(patch.arm("comment"))
        patch.observe_comment(stream="old-recovery", count=1)
        with self.assertRaises(StopClosed):
            patch.finish_patch("old-body", "comment-node")

        for defect in ("future", "incomplete", "wrong-count", "wrong-result"):
            corrupt = copy.deepcopy(patch.record)
            retry = corrupt["comment"]["retry_observation"]
            if defect == "future":
                retry["generation"] = corrupt["generation"] + 1
            elif defect == "incomplete":
                retry["complete"] = False
            elif defect == "wrong-count":
                retry["count"] = 2
            else:
                retry["result_stream"] = "wrong"
            with self.subTest(defect=defect), self.assertRaises(StopClosed):
                ProgramLedgerModel.resume(
                    corrupt, 41, corrupt["self_inode"],
                    hashlib.sha256(ProgramLedgerModel.canonical(corrupt)).hexdigest(),
                    corrupt["authority"],
                )

        invalid = ProgramLedgerModel()
        retained = copy.deepcopy(invalid.record)
        with self.assertRaises(StopClosed):
            invalid.persist(lambda record: record["comment"].update(phase="garbage"))
        self.assertEqual(invalid.record, retained)
        with self.assertRaises(StopClosed):
            invalid.persist(lambda record: record["link"].update(phase="planned"))
        self.assertEqual(invalid.record, retained)

        invalid.observe_comment()
        invalid.plan_comment()
        retained = copy.deepcopy(invalid.record)
        with self.assertRaises(StopClosed):
            invalid.persist(lambda record: record["comment"]["plan_observation"].update(
                generation=record["generation"] + 1
            ))
        self.assertEqual(invalid.record, retained)

        for index, replacement in enumerate(
            ("other-repo", "other-master", "other-goal", "other-actor")
        ):
            authority = ProgramLedgerModel()
            retained = copy.deepcopy(authority.record)
            with self.subTest(authority_index=index), self.assertRaises(StopClosed):
                authority.persist(
                    lambda record, i=index, value=replacement: record["authority"].__setitem__(
                        i, value
                    )
                )
            self.assertEqual(authority.record, retained)

    def test_ordinary_patch_recovers_without_graph_rekey(self) -> None:
        def bound_comment() -> ProgramLedgerModel:
            model = ProgramLedgerModel()
            model.observe_comment()
            model.plan_comment()
            self.refresh_before_arm(model, "comment")
            model.execute(model.arm("comment"))
            self.observe_result(model, "comment")
            model.bind_comment([self.exact_comment()])
            return model

        model = bound_comment()
        original_graph = copy.deepcopy(model.record["graph"])
        model.observe_comment()
        model.plan_comment()
        self.refresh_before_arm(model, "comment")
        model.execute(model.arm("comment"))
        model = ProgramLedgerModel.resume(
            model.record, 41, model.record["self_inode"], model.digest(),
            model.record["authority"], remote_calls=model.remote_calls,
        )
        model.observe_comment(stream="ordinary-old", count=1)
        with self.assertRaises(StopClosed):
            model.finish_patch("drifted-body", "comment-node")
        with self.assertRaises(StopClosed):
            model.finish_patch("old-body", "comment-node")
        model = ProgramLedgerModel.resume(
            model.record, 41, model.record["self_inode"], model.digest(),
            model.record["authority"], remote_calls=model.remote_calls,
        )
        model.observe_comment(stream="ordinary-old", count=1)
        retry = model.finish_patch("old-body", "comment-node")
        self.assertIsNotNone(retry)
        model.execute(retry)
        model = ProgramLedgerModel.resume(
            model.record, 41, model.record["self_inode"], model.digest(),
            model.record["authority"], remote_calls=model.remote_calls,
        )
        model.observe_comment(stream="ordinary-intended", count=1, body="intended-body")
        model.finish_patch("intended-body", "comment-node")
        self.assertEqual(model.record["comment"]["phase"], "bound")
        self.assertEqual(model.record["graph"], original_graph)
        self.assertIsNone(model.record["next_graph"])
        self.assertEqual(model.remote_calls["comment"], 3)

        applied = bound_comment()
        applied.observe_comment()
        applied.plan_comment()
        self.refresh_before_arm(applied, "comment")
        applied.execute(applied.arm("comment"))
        applied = ProgramLedgerModel.resume(
            applied.record, 41, applied.record["self_inode"], applied.digest(),
            applied.record["authority"], remote_calls=applied.remote_calls,
        )
        applied.observe_comment(stream="ordinary-applied", count=1, body="intended-body")
        applied.finish_patch("intended-body", "comment-node")
        self.assertEqual(applied.record["comment"]["phase"], "bound")
        self.assertEqual(applied.remote_calls["comment"], 2)

    def test_marker_namespace_actor_and_duplicates_fail_closed(self) -> None:
        ProgramLedgerModel.validate_marker([], "actor", False)
        ProgramLedgerModel.validate_marker([("marker", "actor")], "actor", True)
        for namespace, bound in (([("old", "actor")], False), ([], True),
                                 ([("a", "actor"), ("b", "actor")], True),
                                 ([("a", "other")], True)):
            with self.subTest(namespace=namespace, bound=bound):
                with self.assertRaises(StopClosed):
                    ProgramLedgerModel.validate_marker(namespace, "actor", bound)

    def test_child_duplicate_classifier_scopes_markers_and_predicates(self) -> None:
        blockers = (
            {"marker": "matching"}, {"title": True}, {"body": True},
            {"backlink": True}, {"parent": True},
        )
        for issue in blockers:
            with self.subTest(issue=issue):
                model = ProgramLedgerModel()
                candidates, _ = model.classify_child([issue])
                self.assertEqual(candidates, [issue])
                with self.assertRaises(StopClosed):
                    model.plan_create()
        model = ProgramLedgerModel()
        unrelated = [{"marker": "unrelated"}, {"marker": "malformed"}]
        candidates, evidence = model.classify_child(unrelated)
        self.assertEqual(candidates, [])
        self.assertEqual(evidence, unrelated)
        model.plan_create()
        with self.assertRaises(StopClosed):
            ProgramLedgerModel().classify_child([], "before", "after")

    def test_crash_before_remote_call_loses_permit_and_never_calls(self) -> None:
        model = ProgramLedgerModel()
        model.observe_comment()
        model.plan_comment()
        self.refresh_before_arm(model, "comment")
        model.arm("comment")
        resumed = ProgramLedgerModel.resume(
            model.record, 41, model.record["self_inode"], model.digest(),
            model.record["authority"],
        )
        with self.assertRaises(StopClosed):
            resumed.arm("comment")
        self.assertEqual(resumed.remote_calls["comment"], 0)

    def test_arm_requires_new_identical_post_plan_observation(self) -> None:
        model = ProgramLedgerModel()
        model.observe_comment(stream="first")
        model.plan_comment()
        with self.assertRaises(StopClosed):
            model.arm("comment")
        model.observe_comment(stream="changed")
        with self.assertRaises(StopClosed):
            model.arm("comment")
        model.observe_comment(stream="first")
        with self.assertRaises(StopClosed):
            model.arm("comment")
        model.observe_comment(stream="first")
        model.arm("comment")

    def test_stable_forbidden_state_never_arms_mutation(self) -> None:
        comment = ProgramLedgerModel()
        comment.observe_comment(count=1)
        with self.assertRaises(StopClosed):
            comment.plan_comment()

        create = ProgramLedgerModel()
        create.classify_child([])
        create.plan_create()
        duplicate = {"title": True}
        create.classify_child([duplicate], "blocked", "blocked")
        with self.assertRaises(StopClosed):
            create.arm("create")
        create.classify_child([], "clear", "clear")
        with self.assertRaises(StopClosed):
            create.arm("create")
        create.classify_child([], "clear", "clear")
        create.arm("create")
        create.classify_child([duplicate], "blocked", "blocked")
        with self.assertRaises(StopClosed):
            create.arm("create")

        link = ProgramLedgerModel()
        link.record["create"].update(
            phase="bound", node="issue-node", created_at="2026-08-14T00:00:00Z"
        )
        link.observe_parent()
        link.plan_link()
        link.observe_parent(["issue-node"], stream="blocked")
        with self.assertRaises(StopClosed):
            link.arm("link")
        link.observe_parent(["issue-node"], stream="blocked")
        with self.assertRaises(StopClosed):
            link.arm("link")

    def test_crash_after_each_remote_call_never_reposts(self) -> None:
        for slot in ("comment", "create", "link"):
            with self.subTest(slot=slot):
                model = ProgramLedgerModel()
                if slot == "comment":
                    model.observe_comment()
                    model.plan_comment()
                elif slot == "create":
                    model.classify_child([])
                    model.plan_create()
                else:
                    model.record["create"].update(
                        phase="bound", node="issue-node",
                        created_at="2026-08-14T00:00:00Z",
                    )
                    model.observe_parent()
                    model.plan_link()
                self.refresh_before_arm(model, slot)
                permit = model.arm(slot)
                model.execute(permit)
                with self.assertRaises(StopClosed):
                    model.execute(permit)
                resumed = ProgramLedgerModel.resume(
                    model.record, 41, model.record["self_inode"], model.digest(),
                    model.record["authority"], remote_calls=model.remote_calls,
                )
                with self.assertRaises(StopClosed):
                    resumed.arm(slot)
                self.assertEqual(model.remote_calls[slot], 1)

    def test_invisible_post_can_bind_later_from_persisted_intent(self) -> None:
        for report_present in (False, True):
            with self.subTest(report_present=report_present):
                model = ProgramLedgerModel()
                model.observe_comment()
                model.plan_comment()
                self.refresh_before_arm(model, "comment")
                permit = model.arm("comment")
                model.execute(permit)
                resumed = ProgramLedgerModel.resume(
                    model.record, 41, model.record["self_inode"], model.digest(),
                    model.record["authority"], report_present=report_present,
                    remote_calls=model.remote_calls,
                )
                self.assertEqual(resumed.report_present, report_present)
                with self.assertRaises(StopClosed):
                    resumed.arm("comment")
                self.observe_result(resumed, "comment")
                resumed.bind_comment([self.exact_comment()])
                self.assertEqual(resumed.record["comment"]["phase"], "bound")
                self.assertEqual(resumed.remote_calls["comment"], 1)

    def test_invisible_child_create_and_link_bind_after_restart(self) -> None:
        model = ProgramLedgerModel()
        model.classify_child([])
        model.plan_create()
        self.refresh_before_arm(model, "create")
        create = model.arm("create")
        model.execute(create)
        resumed = ProgramLedgerModel.resume(
            model.record, 41, model.record["self_inode"], model.digest(),
            model.record["authority"], remote_calls=model.remote_calls,
        )
        self.observe_result(resumed, "create")
        resumed.bind_create([self.exact_child()])
        resumed.observe_parent()
        resumed.plan_link()
        self.refresh_before_arm(resumed, "link")
        link = resumed.arm("link")
        resumed.execute(link)
        again = ProgramLedgerModel.resume(
            resumed.record, 41, resumed.record["self_inode"], resumed.digest(),
            resumed.record["authority"], remote_calls=resumed.remote_calls,
        )
        self.observe_result(again, "link")
        again.bind_link("master", ["issue-node"])
        self.assertEqual(again.remote_calls, {"comment": 0, "create": 1, "link": 1})

    def test_stale_generation_digest_and_lock_writers_stop(self) -> None:
        model = ProgramLedgerModel()
        generation, digest = model.record["generation"], model.digest()
        model.persist(lambda record: None, generation, digest)
        for args in ((generation, model.digest(), 41),
                     (model.record["generation"], digest, 41),
                     (model.record["generation"], model.digest(), 99)):
            with self.subTest(args=args), self.assertRaises(StopClosed):
                model.persist(lambda record: None, *args)
        model = ProgramLedgerModel()
        model.persist(lambda record: None)
        retained = copy.deepcopy(model.record)
        for value in (True, False):
            with self.subTest(expected_generation=value), self.assertRaises(StopClosed):
                model.persist(lambda record: None, expected_generation=value)
            self.assertEqual(model.record, retained)
        for value in (True, False):
            with self.subTest(leaf_generation=value), self.assertRaises(StopClosed):
                model.compose_leaf("leaf-1", value, "a" * 64)

    def test_patch_and_rekey_keep_one_comment_node_and_authority(self) -> None:
        model = ProgramLedgerModel()
        model.observe_comment()
        model.plan_comment()
        self.refresh_before_arm(model, "comment")
        permit = model.arm("comment")
        model.execute(permit)
        self.observe_result(model, "comment")
        model.bind_comment([self.exact_comment()])
        model.observe_comment(complete=False)
        with self.assertRaises(StopClosed):
            model.rekey(
                model.record["authority"], ["leaf-1", "leaf-2"], "comment-node"
            )
        model.observe_comment()
        model.rekey(model.record["authority"], ["leaf-1", "leaf-2"], "comment-node")
        for namespace in ([], [("marker", "other")],
                          [("marker", "actor"), ("second", "actor")],
                          [("wrong-node", "actor", "program-marker", "old-body")],
                          [("comment-node", "actor", "wrong-marker", "old-body")]):
            with self.subTest(namespace=namespace), self.assertRaises(StopClosed):
                model.observe_comment(namespace=namespace, count=len(namespace))
        self.refresh_before_arm(model, "comment")
        patch = model.arm("comment")
        model.execute(patch)
        model = ProgramLedgerModel.resume(
            model.record, 41, model.record["self_inode"], model.digest(),
            model.record["authority"], remote_calls=model.remote_calls,
        )
        model.observe_comment(stream="patch-old", count=1)
        with self.assertRaises(StopClosed):
            model.finish_patch("drifted-body", "comment-node")
        with self.assertRaises(StopClosed):
            model.finish_patch("old-body", "comment-node")
        model.observe_comment(stream="patch-old", count=1)
        retry = model.finish_patch("old-body", "comment-node")
        self.assertIsNotNone(retry)
        self.assertEqual(model.record["graph"], ["leaf-1"])
        self.assertEqual(model.record["next_graph"], ["leaf-1", "leaf-2"])
        model.execute(retry)
        model = ProgramLedgerModel.resume(
            model.record, 41, model.record["self_inode"], model.digest(),
            model.record["authority"], remote_calls=model.remote_calls,
        )
        model.observe_comment(stream="patch-intended", count=1, body="intended-body")
        model.finish_patch("intended-body", "comment-node")
        self.assertEqual(model.record["comment"]["node"], "comment-node")
        self.assertEqual(model.record["graph"], ["leaf-1", "leaf-2"])
        self.assertIsNone(model.record["next_graph"])
        with self.assertRaises(StopClosed):
            model.rekey(["other"] * 4, ["bad"], "comment-node")

    def test_create_result_requires_exact_actor_bytes_and_child_marker(self) -> None:
        model = ProgramLedgerModel()
        model.classify_child([])
        model.plan_create()
        self.refresh_before_arm(model, "create")
        permit = model.arm("create")
        model.execute(permit)
        wrong = dict(self.exact_child(), creator="other")
        with self.assertRaises(StopClosed):
            wrong_stream = model.result_stream([wrong])
            model.classify_child([wrong], wrong_stream, wrong_stream)
            model.bind_create([wrong])
        with self.assertRaises(StopClosed):
            model.bind_create([self.exact_child()])
        exact = dict(wrong, creator="actor")
        exact_stream = model.result_stream([exact])
        model.classify_child([exact], exact_stream, exact_stream)
        model.bind_create([exact])
        self.assertEqual(model.record["create"]["created_at"], exact["created_at"])

        for timestamp in (
            "", "2026-08-14T00:00:00", "2026-08-14T00:00:00+01:00",
            "not-a-time",
        ):
            with self.subTest(timestamp=timestamp):
                rejected = ProgramLedgerModel()
                rejected.classify_child([])
                rejected.plan_create()
                self.refresh_before_arm(rejected, "create")
                rejected.execute(rejected.arm("create"))
                child = dict(self.exact_child(), created_at=timestamp)
                stream = rejected.result_stream([child])
                rejected.classify_child([child], stream, stream)
                with self.assertRaises(StopClosed):
                    rejected.bind_create([child])

    def test_native_link_binds_parent_child_pair_without_edge_node(self) -> None:
        model = ProgramLedgerModel()
        with self.assertRaises(StopClosed):
            model.plan_link()
        model.record["create"].update(
            phase="bound", node="issue-node", created_at="2026-08-14T00:00:00Z"
        )
        model.observe_parent()
        model.plan_link()
        self.refresh_before_arm(model, "link")
        permit = model.arm("link")
        model.execute(permit)
        with self.assertRaises(StopClosed):
            model.observe_parent(["issue-node"], stream="parent-wrong")
            model.bind_link("wrong-parent", ["issue-node"])
        model.observe_parent(["other-child"], stream="parent-other")
        with self.assertRaises(StopClosed):
            model.bind_link("master", ["issue-node"])
        model.observe_parent(["issue-node"], stream="parent-exact")
        model.bind_link("master", ["issue-node"])
        self.assertEqual(
            (model.record["link"]["parent"], model.record["link"]["child"]),
            ("master", "issue-node"),
        )

    def test_duplicate_uncertain_results_stop_without_repost(self) -> None:
        model = ProgramLedgerModel()
        model.classify_child([])
        model.plan_create()
        self.refresh_before_arm(model, "create")
        permit = model.arm("create")
        model.execute(permit)
        for results in ([], [self.exact_child(), self.exact_child()]):
            with self.subTest(results=results), self.assertRaises(StopClosed):
                stream = model.result_stream(results)
                model.classify_child(results, stream, stream)
                model.bind_create(results)
        self.assertEqual(model.remote_calls["create"], 1)

    def test_exact_result_plus_conflict_never_binds(self) -> None:
        model = ProgramLedgerModel()
        model.observe_comment()
        model.plan_comment()
        self.refresh_before_arm(model, "comment")
        permit = model.arm("comment")
        model.execute(permit)
        wrong = dict(self.exact_comment(), author="other")
        results = [self.exact_comment(), wrong]
        model.observe_comment(stream=model.result_stream(results), count=2)
        with self.assertRaises(StopClosed):
            model.bind_comment(results)

    def test_create_and_link_cannot_plan_twice_or_out_of_order(self) -> None:
        model = ProgramLedgerModel()
        with self.assertRaises(StopClosed):
            model.plan_create()
        with self.assertRaises(StopClosed):
            model.plan_link()
        model.classify_child([])
        model.plan_create()
        self.refresh_before_arm(model, "create")
        create_permit = model.arm("create")
        model.execute(create_permit)
        self.observe_result(model, "create")
        model.bind_create([self.exact_child()])
        with self.assertRaises(StopClosed):
            model.plan_create()
        model.observe_parent()
        model.plan_link()
        self.refresh_before_arm(model, "link")
        link_permit = model.arm("link")
        model.execute(link_permit)
        self.observe_result(model, "link")
        model.bind_link("master", ["issue-node"])
        with self.assertRaises(StopClosed):
            model.plan_link()
        self.assertEqual(model.remote_calls, {"comment": 0, "create": 1, "link": 1})

    def test_leaf_composition_rejects_overlap_reorder_and_drift(self) -> None:
        model = ProgramLedgerModel()
        model.compose_leaf("leaf-1", 1, "a" * 64)
        for args in (("leaf-2", 1, "a" * 64), ("leaf-1", 2, "a" * 64),
                     ("leaf-1", 1, "b" * 64)):
            with self.subTest(args=args), self.assertRaises(StopClosed):
                model.compose_leaf(*args)
        for graph in (["leaf-1", "leaf-1"], [""]):
            corrupt = copy.deepcopy(model.record)
            corrupt["graph"] = graph
            if graph == [""]:
                corrupt["leaf_snapshots"] = {}
            with self.subTest(graph=graph), self.assertRaises(StopClosed):
                ProgramLedgerModel.resume(
                    corrupt, 41, corrupt["self_inode"],
                    hashlib.sha256(ProgramLedgerModel.canonical(corrupt)).hexdigest(),
                    corrupt["authority"],
                )

        rekey = ProgramLedgerModel()
        rekey.observe_comment()
        rekey.plan_comment()
        self.refresh_before_arm(rekey, "comment")
        rekey.execute(rekey.arm("comment"))
        self.observe_result(rekey, "comment")
        rekey.bind_comment([self.exact_comment()])
        rekey.observe_comment()
        for graph in (["leaf-1", "leaf-1"], [""]):
            retained = copy.deepcopy(rekey.record)
            with self.subTest(rekey=graph), self.assertRaises(StopClosed):
                rekey.rekey(rekey.record["authority"], graph, "comment-node")
            self.assertEqual(rekey.record, retained)


if __name__ == "__main__":
    unittest.main()
