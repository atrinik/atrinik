from __future__ import annotations

import unittest

from scripts.benchmark_devcontainer_storage import (
    DEFAULT_WINDOWS_IMAGE,
    WORKLOAD_SCRIPT,
    _parse_workload_output,
    _volume_name,
    BenchmarkError,
    _validate_image,
    _validate_run_id,
)


class StorageBenchmarkTests(unittest.TestCase):
    def test_default_image_is_digest_pinned(self) -> None:
        self.assertEqual(_validate_image(DEFAULT_WINDOWS_IMAGE), DEFAULT_WINDOWS_IMAGE)

    def test_workload_output_is_bounded_and_typed(self) -> None:
        parsed = _parse_workload_output(
            "created=32 reused=0 bytes=123 digest=" + "a" * 64 + "\n"
        )

        self.assertEqual(parsed["created"], 32)
        self.assertEqual(parsed["reused"], 0)
        self.assertEqual(parsed["bytes"], 123)
        self.assertEqual(parsed["digest"], "a" * 64)
        self.assertIn("cache", WORKLOAD_SCRIPT)
        self.assertEqual(
            _volume_name("dev-abc123", "test123"),
            "atrinik-dev-abc123-benchmark-test123-volume",
        )

    def test_run_id_rejects_path_syntax(self) -> None:
        with self.assertRaisesRegex(BenchmarkError, "Docker-safe"):
            _validate_run_id("../unsafe")
