from __future__ import annotations

from pathlib import Path
import unittest

from atrinik_workspace.jsonc import loads as jsonc_loads


ROOT = Path(__file__).resolve().parents[1]


class DevcontainerTests(unittest.TestCase):
    def load_config(self, relative_path: str) -> dict[str, object]:
        return jsonc_loads((ROOT / relative_path).read_text(encoding="utf-8"))

    def test_default_configuration_initializes_component_checkouts(self) -> None:
        config = self.load_config(".devcontainer/devcontainer.json")

        self.assertEqual(config["workspaceFolder"], "/workspaces/atrinik")
        self.assertEqual(config["postCreateCommand"], "./atrinik init")
        self.assertEqual(
            config["image"],
            "ghcr.io/atrinik/linux-build:1.3.0@sha256:"
            "260658d2709e993b41148a9d8f724c2d2f7f1fd93543a139b00d139b10e7f31a",
        )
        self.assertEqual(config["containerEnv"]["SDL_VIDEODRIVER"], "x11")

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
            "ghcr.io/atrinik/windows-build:1.2.1@sha256:"
            "d1f082eb28891600a9cf018a1d4310b9f3e1f985f82139fa48fbd4ac77b623bb",
        )

    def test_default_feature_lock_matches_configuration(self) -> None:
        config = self.load_config(".devcontainer/devcontainer.json")
        lock = self.load_config(".devcontainer/devcontainer-lock.json")

        self.assertEqual(set(config["features"]), set(lock["features"]))

    def test_editor_excludes_generated_workspace_state(self) -> None:
        default_config = self.load_config(".devcontainer/devcontainer.json")
        windows_config = self.load_config(
            ".devcontainer/windows-cross/devcontainer.json"
        )
        workspace_settings = self.load_config(".vscode/settings.json")
        expected_watcher_excludes = {
            "workspace/**": True,
            "build/**": True,
        }

        for config in (default_config, windows_config):
            settings = config["customizations"]["vscode"]["settings"]
            self.assertEqual(
                settings["files.watcherExclude"],
                expected_watcher_excludes,
            )

        self.assertEqual(
            workspace_settings["files.watcherExclude"],
            expected_watcher_excludes,
        )

        pyright_config = self.load_config("pyrightconfig.json")
        self.assertEqual(
            pyright_config["exclude"],
            [
                "workspace",
                "build",
                "**/node_modules",
                "**/__pycache__",
                "**/.*",
            ],
        )


if __name__ == "__main__":
    unittest.main()
