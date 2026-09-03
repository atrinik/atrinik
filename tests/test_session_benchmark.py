from __future__ import annotations

import unittest

from scripts.benchmark_devcontainer_session import (
    DEFAULT_LINUX_IMAGE,
    SESSION_WORKLOAD_SCRIPT,
    BenchmarkError,
    _comparison,
    _container_name,
    _parse_workload_output,
    _validate_image,
    _validate_run_id,
    _volume_name,
)


class SessionBenchmarkTests(unittest.TestCase):
    def test_default_image_is_digest_pinned(self) -> None:
        self.assertEqual(_validate_image(DEFAULT_LINUX_IMAGE), DEFAULT_LINUX_IMAGE)

    def test_workload_output_is_bounded_and_typed(self) -> None:
        parsed = _parse_workload_output(
            "created=32 reused=0 bytes=123 digest=" + "a" * 64 + "\n"
        )

        self.assertEqual(parsed["created"], 32)
        self.assertEqual(parsed["reused"], 0)
        self.assertEqual(parsed["bytes"], 123)
        self.assertEqual(parsed["digest"], "a" * 64)
        self.assertIn("reused", SESSION_WORKLOAD_SCRIPT)

    def test_resource_names_are_unique_and_docker_safe(self) -> None:
        warm_volume = _volume_name("dev-abc123", "test123", "warm")
        parallel_volume = _volume_name("dev-abc123", "test123", "parallel-00")
        warm_container = _container_name("dev-abc123", "test123", "warm")

        self.assertEqual(
            warm_volume,
            "atrinik-dev-abc123-session-test123-warm-volume",
        )
        self.assertNotEqual(warm_volume, parallel_volume)
        self.assertIn("session-test123-warm", warm_container)

    def test_run_id_rejects_path_syntax(self) -> None:
        with self.assertRaisesRegex(BenchmarkError, "Docker-safe"):
            _validate_run_id("../unsafe")

    def test_comparison_reports_reuse_and_parallel_speedup(self) -> None:
        cold = [{"duration_ms": 100.0}, {"duration_ms": 120.0}]
        warm = [{"duration_ms": 20.0}, {"duration_ms": 24.0}]
        parallel = [
            {"duration_ms": 40.0},
            {"duration_ms": 45.0},
        ]

        comparison = _comparison(cold, warm, 50.0, parallel)

        self.assertEqual(comparison["cold_median_ms"], 110.0)
        self.assertEqual(comparison["warm_median_ms"], 22.0)
        self.assertEqual(comparison["parallel_serial_work_ms"], 85.0)
        self.assertEqual(comparison["parallel_speedup_vs_serial"], 1.7)
