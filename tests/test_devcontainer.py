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
        self.assertIn(
            "${localWorkspaceFolder}/workspace/build",
            config["initializeCommand"],
        )
        self.assertIn(
            "source=atrinik-${devcontainerId}-build-cache,"
            "target=/workspaces/atrinik/workspace/build,type=volume,volume-nocopy",
            config["mounts"],
        )
        self.assertIn(
            "sudo chown -R --no-dereference ubuntu:ubuntu",
            config["onCreateCommand"],
        )
        self.assertEqual(
            config["containerEnv"]["ATRINIK_DOCKER_VOLUME_NAMESPACE"],
            "${devcontainerId}",
        )
        self.assertEqual(
            config["image"],
            "ghcr.io/atrinik/linux-build:1.10.0@sha256:"
            "7904a1802054662b0ede5b55de72e4c92b0112a3c211125f994ed6c62e9ec9d8",
        )
        self.assertNotIn("SDL_VIDEODRIVER", config["containerEnv"])

    def test_linux_default_has_no_graphics_or_privileged_bootstrap(self) -> None:
        config = self.load_config(".devcontainer/devcontainer.json")
        self.assertEqual(config["runArgs"], [])
        self.assertEqual(config["features"], {})
        serialized = str(config)
        for forbidden in ("/mnt/wslg", "/dev/dxg", "--gpus", "--privileged",
                          "seccomp=unconfined", "DISPLAY", "PULSE_SERVER"):
            with self.subTest(forbidden=forbidden):
                self.assertNotIn(forbidden, serialized)

    def test_wslg_is_explicit_and_preserves_its_graphics_contract(self) -> None:
        base = self.load_config(".devcontainer/devcontainer.json")
        config = self.load_config(".devcontainer/windows-wslg/devcontainer.json")
        self.assertEqual(config["image"], base["image"])
        self.assertIn("--device=/dev/dxg", config["runArgs"])
        self.assertEqual(config["containerEnv"]["SDL_VIDEODRIVER"], "x11")
        self.assertEqual(config["containerEnv"]["PULSE_SERVER"],
                         "unix:/mnt/wslg/PulseServer")
        lock = self.load_config(".devcontainer/windows-wslg/devcontainer-lock.json")
        self.assertEqual(set(config["features"]), set(lock["features"]))
        for feature in lock["features"].values():
            self.assertIn("@sha256:", feature["resolved"])

    def test_shared_github_auth_is_read_only_and_coordinator_only(self) -> None:
        config = self.load_config(".devcontainer/devcontainer.json")
        auth_dir = config["containerEnv"]["GH_CONFIG_DIR"]
        mounts = [
            mount.split(",")
            for mount in config["mounts"]
            if f"target={auth_dir}" in mount.split(",")
        ]
        self.assertEqual(len(mounts), 1)
        self.assertIn("type=bind", mounts[0])
        self.assertIn("readonly", mounts[0])
        self.assertIn(
            "source=${localEnv:HOME}/.config/gh-atrinik", mounts[0]
        )
        self.assertEqual(auth_dir, f"/home/{config['remoteUser']}/.config/gh")
        self.assertNotEqual(auth_dir, config["containerEnv"]["CODEX_HOME"])
        self.assertFalse(
            {"GH_TOKEN", "GITHUB_TOKEN"} & config["containerEnv"].keys()
        )
        windows = self.load_config(".devcontainer/windows-cross/devcontainer.json")
        self.assertNotIn("GH_CONFIG_DIR", windows.get("containerEnv", {}))
        self.assertFalse(any("gh-atrinik" in mount for mount in windows["mounts"]))

    def test_windows_configuration_validates_component_manifest(self) -> None:
        config = self.load_config(
            ".devcontainer/windows-cross/devcontainer.json"
        )

        self.assertEqual(config["workspaceFolder"], "/workspaces/atrinik")
        self.assertEqual(
            config["postCreateCommand"], "./atrinik manifest validate"
        )
        self.assertIn(
            "${localWorkspaceFolder}/workspace/build",
            config["initializeCommand"],
        )
        self.assertIn(
            "source=atrinik-${devcontainerId}-build-cache,"
            "target=/workspaces/atrinik/workspace/build,type=volume,volume-nocopy",
            config["mounts"],
        )
        self.assertIn(
            "sudo chown -R --no-dereference vscode:vscode",
            config["onCreateCommand"],
        )
        self.assertEqual(
            config["containerEnv"]["ATRINIK_DOCKER_VOLUME_NAMESPACE"],
            "${devcontainerId}",
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
