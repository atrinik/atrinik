from __future__ import annotations

import os
from pathlib import Path
from types import SimpleNamespace
import unittest
from unittest import mock

from atrinik_workspace import process_tree


class ProcessTreeIdentityTests(unittest.TestCase):
    def test_malformed_fdinfo_is_ignored(self) -> None:
        descriptor = mock.Mock()
        descriptor.name = "7"
        descriptor.stat.return_value = SimpleNamespace(st_dev=11, st_ino=22)

        for fdinfo in ("position:\t0\n", "flags:\tinvalid\n"):
            with self.subTest(fdinfo=fdinfo):
                with (
                    mock.patch.object(Path, "iterdir", return_value=[descriptor]),
                    mock.patch.object(Path, "read_text", return_value=fdinfo),
                ):
                    self.assertFalse(process_tree._holds_identity(123, (11, 22)))

    def test_observer_descriptor_is_not_a_process_tree_holder(self) -> None:
        descriptor = mock.Mock()
        descriptor.name = "7"
        descriptor.stat.return_value = SimpleNamespace(st_dev=11, st_ino=22)

        with (
            mock.patch.object(Path, "iterdir", return_value=[descriptor]),
            mock.patch.object(
                Path,
                "read_text",
                return_value=f"flags:\t{getattr(os, 'O_PATH', 0):o}\n",
            ),
        ):
            self.assertFalse(process_tree._holds_identity(123, (11, 22)))

        with (
            mock.patch.object(Path, "iterdir", return_value=[descriptor]),
            mock.patch.object(Path, "read_text", return_value="flags:\t0\n"),
        ):
            self.assertTrue(process_tree._holds_identity(123, (11, 22)))


if __name__ == "__main__":
    unittest.main()
