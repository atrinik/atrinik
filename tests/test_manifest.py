from __future__ import annotations

import json
from pathlib import Path, PurePosixPath
import tempfile
import unittest

from atrinik_workspace.model import Manifest, WorkspaceError


ROOT = Path(__file__).resolve().parents[1]


class ManifestClosureTests(unittest.TestCase):
    def _classic_manifest(self) -> dict[str, object]:
        return json.loads((ROOT / "components.json").read_text(encoding="utf-8"))

    def _load_payload(self, payload: dict[str, object]) -> Manifest:
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "components.json"
            path.write_text(json.dumps(payload), encoding="utf-8")
            return Manifest.load(path)

    @staticmethod
    def _component(payload: dict[str, object], name: str) -> dict[str, object]:
        return next(
            component for component in payload["components"]
            if component["name"] == name
        )

    def test_classic_client_can_capture_strict_server_descendants_in_both_orders(self) -> None:
        for include in (
            "server/dependencies.lock.json",
            "server/declared-inputs",
        ):
            for client_first in (True, False):
                with self.subTest(include=include, client_first=client_first):
                    payload = self._classic_manifest()
                    client = self._component(payload, "classic-client")
                    server = self._component(payload, "classic-server")
                    client["source_includes"] = [
                        "cmake", "LICENSE.md", "ATTRIBUTIONS.md",
                        include,
                    ]
                    components = payload["components"]
                    client_index = components.index(client)
                    server_index = components.index(server)
                    if client_first != (client_index < server_index):
                        components[client_index], components[server_index] = (
                            components[server_index], components[client_index]
                        )

                    manifest = self._load_payload(payload)
                    self.assertIn(
                        include,
                        manifest.by_name["classic-client"].source_includes,
                    )

    def test_classic_peer_include_rejects_non_descendant_overlap_shapes(self) -> None:
        cases = {
            "peer-equality": ("client", ["server"]),
            "peer-ancestor": ("client", ["."]),
            "own-source": ("client", ["client/private.lock"]),
            "source-source": ("server/client", []),
            "unsafe-path": ("client", ["../server/dependencies.lock.json"]),
        }
        for name, (source, includes) in cases.items():
            with self.subTest(case=name):
                payload = self._classic_manifest()
                client = self._component(payload, "classic-client")
                client["source"] = source
                client["source_includes"] = includes
                with self.assertRaises(WorkspaceError):
                    self._load_payload(payload)

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
