from __future__ import annotations

from pathlib import Path, PurePosixPath
import unittest

from atrinik_workspace.model import Manifest


ROOT = Path(__file__).resolve().parents[1]


class ManifestClosureTests(unittest.TestCase):
    def test_classic_protocol_and_library_share_root_cmake_closure(self) -> None:
        manifest = Manifest.load(ROOT / "components.json")
        components = {
            name: manifest.by_name[name]
            for name in ("classic-protocol", "classic-libatrinik")
        }

        self.assertEqual(
            {name: component.source for name, component in components.items()},
            {"classic-protocol": "protocol", "classic-libatrinik": "libatrinik"},
        )
        self.assertTrue(
            all(component.source_includes == ("cmake",)
                for component in components.values())
        )

        shared_cmake = PurePosixPath("cmake")
        for component in components.values():
            source = PurePosixPath(component.source)
            self.assertNotEqual(source, shared_cmake)
            self.assertNotIn(source, shared_cmake.parents)
            self.assertNotIn(shared_cmake, source.parents)


if __name__ == "__main__":
    unittest.main()
