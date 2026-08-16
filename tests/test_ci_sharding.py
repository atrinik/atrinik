from __future__ import annotations

import json
from pathlib import Path
from types import SimpleNamespace
import tempfile
import unittest
from unittest import mock

from atrinik_workspace import ci_sharding


class CiShardingTests(unittest.TestCase):
    def weights(self) -> dict[str, object]:
        return {
            "baseline": {},
            "default_seconds": 1.0,
            "prefix_seconds": [
                {"prefix": "tests.slow.", "seconds": 4.0},
            ],
            "schema_version": 1,
            "tests": {"tests.fast.outlier": 8.0},
        }

    def test_assignment_is_deterministic_and_uses_measured_weights(self) -> None:
        test_ids = [
            "tests.slow.one",
            "tests.slow.two",
            "tests.fast.one",
            "tests.fast.two",
            "tests.fast.outlier",
        ]
        first = ci_sharding.assign_tests(test_ids, self.weights(), 2)
        second = ci_sharding.assign_tests(list(reversed(test_ids)), self.weights(), 2)

        self.assertEqual(first, second)
        self.assertNotEqual(
            next(index for index, shard in enumerate(first) if "tests.fast.outlier" in shard),
            next(index for index, shard in enumerate(first) if "tests.slow.one" in shard),
        )
        self.assertEqual(sorted(test for shard in first for test in shard), sorted(test_ids))

    def test_weight_validation_rejects_duplicate_or_nonpositive_entries(self) -> None:
        with tempfile.TemporaryDirectory() as temporary_name:
            path = Path(temporary_name) / "weights.json"
            invalid = self.weights()
            invalid["prefix_seconds"] = [
                {"prefix": "tests.", "seconds": 1.0},
                {"prefix": "tests.", "seconds": 2.0},
            ]
            path.write_text(json.dumps(invalid), encoding="utf-8")
            with self.assertRaisesRegex(ValueError, "unique nonempty prefixes"):
                ci_sharding.load_weights(path)

    def test_verify_requires_every_discovered_test_exactly_once(self) -> None:
        test_ids = ["tests.example.one", "tests.example.two", "tests.example.three"]
        weights = self.weights()
        with tempfile.TemporaryDirectory() as temporary_name:
            root = Path(temporary_name)
            weights_path = root / "weights.json"
            weights_path.write_text(json.dumps(weights), encoding="utf-8")
            assignments = ci_sharding.assign_tests(test_ids, weights, 2)
            assignment_digest = ci_sharding._canonical_digest(
                {str(index): tests for index, tests in enumerate(assignments)}
            )
            for index, selected in enumerate(assignments):
                ci_sharding._write_json(
                    root / f"shard-{index}-manifest.json",
                    {
                        "assignments_sha256": assignment_digest,
                        "discovered_count": len(test_ids),
                        "discovered_sha256": ci_sharding._canonical_digest(test_ids),
                        "schema_version": 1,
                        "selected_ids": selected,
                        "shard_count": 2,
                        "shard_index": index,
                        "timing_weights_sha256": ci_sharding._canonical_digest(weights),
                    },
                )
            arguments = SimpleNamespace(
                manifests=root,
                output=root / "verified.json",
                shard_count=2,
                weights=weights_path,
            )
            with mock.patch.object(ci_sharding, "discover_test_ids", return_value=test_ids):
                self.assertEqual(ci_sharding.verify_shards(arguments), 0)
            self.assertTrue(arguments.output.is_file())

            duplicate = json.loads((root / "shard-1-manifest.json").read_text())
            duplicate["selected_ids"].append(assignments[0][0])
            duplicate["selected_ids"].sort()
            ci_sharding._write_json(root / "shard-1-manifest.json", duplicate)
            with mock.patch.object(ci_sharding, "discover_test_ids", return_value=test_ids):
                with self.assertRaisesRegex(ValueError, "deterministic assignment"):
                    ci_sharding.verify_shards(arguments)

            duplicate["selected_ids"] = assignments[1]
            duplicate["shard_index"] = 2
            ci_sharding._write_json(root / "shard-1-manifest.json", duplicate)
            with mock.patch.object(ci_sharding, "discover_test_ids", return_value=test_ids):
                with self.assertRaisesRegex(ValueError, "invalid shard index"):
                    ci_sharding.verify_shards(arguments)


if __name__ == "__main__":
    unittest.main()
