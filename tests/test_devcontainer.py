from __future__ import annotations

import json
from pathlib import Path
import unittest


ROOT = Path(__file__).resolve().parents[1]


class DevcontainerTests(unittest.TestCase):
    def load_config(self, relative_path: str) -> dict[str, object]:
        with (ROOT / relative_path).open(encoding="utf-8") as config_file:
            return json.load(config_file)

    def test_default_configuration_initializes_component_checkouts(self) -> None:
        config = self.load_config(".devcontainer/devcontainer.json")

        self.assertEqual(config["workspaceFolder"], "/workspaces/atrinik")
        self.assertEqual(config["postCreateCommand"], "./atrinik init")
        self.assertEqual(
            config["image"],
            "ghcr.io/atrinik/linux-build:1.0.5@sha256:"
            "be3427cfc7dabcf837450c7306d55883c32e20ba1ab7cc96b4da4b966b8066de",
        )

    def test_windows_configuration_validates_component_manifest(self) -> None:
        config = self.load_config(
            ".devcontainer/windows-cross/devcontainer.json"
        )

        self.assertEqual(config["workspaceFolder"], "/workspaces/atrinik")
        self.assertEqual(
            config["postCreateCommand"], "./atrinik manifest validate"
        )
        self.assertEqual(
            config["image"],
            "ghcr.io/atrinik/windows-build:1.0.5@sha256:"
            "9cc373f620a577328fc0a7a7fa823bddaca6d7dc75ac73bcf21be421c49676f7",
        )

    def test_default_feature_lock_matches_configuration(self) -> None:
        config = self.load_config(".devcontainer/devcontainer.json")
        lock = self.load_config(".devcontainer/devcontainer-lock.json")

        self.assertEqual(set(config["features"]), set(lock["features"]))


if __name__ == "__main__":
    unittest.main()
