from __future__ import annotations

import importlib.util
import copy
import json
from pathlib import Path
import unittest


ROOT = Path(__file__).resolve().parents[1]
PROFILE_ROOT = ROOT / "mcp" / "external-profiles"


def load_validator():
    specification = importlib.util.spec_from_file_location(
        "mcp_external_profile_validator", PROFILE_ROOT / "validate.py"
    )
    if specification is None or specification.loader is None:
        raise RuntimeError("external profile validator is unavailable")
    module = importlib.util.module_from_spec(specification)
    specification.loader.exec_module(module)
    return module


class McpExternalProfileTests(unittest.TestCase):
    def setUp(self) -> None:
        self.validator = load_validator()
        self.document = json.loads(
            (PROFILE_ROOT / "profiles.json").read_text(encoding="utf-8")
        )
        self.acceptance = json.loads(
            (PROFILE_ROOT / "fixtures" / "acceptance.json").read_text(
                encoding="utf-8"
            )
        )
        self.template = (
            PROFILE_ROOT / "templates" / "codex.config.toml.example"
        ).read_text(encoding="utf-8")

    def test_profiles_pass_offline_safety_validation(self) -> None:
        result = self.validator.validate()
        self.assertEqual(result["profiles"], 4)
        self.assertEqual(result["acceptance_cases"], 4)
        self.assertEqual(result["adversarial_cases"], 8)

    def test_templates_are_opt_in_and_secret_free(self) -> None:
        template = (PROFILE_ROOT / "templates" / "codex.config.toml.example").read_text(
            encoding="utf-8"
        )
        self.assertEqual(template.count("enabled = false"), 3)
        self.assertNotIn("TOKEN =", template)
        self.assertNotIn("PASSWORD =", template)
        self.assertNotIn("API_KEY =", template)

    def test_machine_decisions_match_safety_boundary(self) -> None:
        profiles = {profile["id"]: profile for profile in self.document["profiles"]}
        self.assertFalse(self.document["live_acceptance"])
        self.assertEqual(profiles["cloudflare-operational"]["decision"], "reject")
        self.assertIn(
            "--read-only", profiles["github-read-only"]["required_flags"]
        )
        self.assertIn(
            "--disable-write",
            profiles["grafana-prometheus-dashboard-read-only"]["required_flags"],
        )
        self.assertNotIn(
            "loki",
            " ".join(
                profiles["grafana-prometheus-dashboard-read-only"]["catalog"]["tools"]
            ).lower(),
        )

    def test_unsafe_profile_mutations_fail_closed(self) -> None:
        mutations = []

        live = copy.deepcopy(self.document)
        live["live_acceptance"] = True
        mutations.append(live)

        cloudflare = copy.deepcopy(self.document)
        rejected = next(
            profile
            for profile in cloudflare["profiles"]
            if profile["id"] == "cloudflare-operational"
        )
        rejected["decision"] = "configure"
        rejected["catalog"]["tools"] = ["execute"]
        rejected["catalog"]["maximum_visible_tools"] = 1
        mutations.append(cloudflare)

        grafana = copy.deepcopy(self.document)
        grafana_profile = next(
            profile
            for profile in grafana["profiles"]
            if profile["id"] == "grafana-prometheus-dashboard-read-only"
        )
        grafana_profile["catalog"]["tools"][0] = "update_dashboard"
        mutations.append(grafana)

        browser = copy.deepcopy(self.document)
        browser_profile = next(
            profile
            for profile in browser["profiles"]
            if profile["id"] == "browser-verification"
        )
        browser_profile["required_flags"].remove("--isolated=true")
        mutations.append(browser)

        for document in mutations:
            with self.subTest(document=document):
                with self.assertRaises(self.validator.ProfileError):
                    self.validator.validate_documents(
                        document, self.acceptance, self.template
                    )

    def test_credential_and_non_synthetic_evidence_fail_closed(self) -> None:
        with self.assertRaises(self.validator.ProfileError):
            self.validator.validate_documents(
                self.document,
                self.acceptance,
                self.template + "\nGITHUB_PERSONAL_ACCESS_TOKEN = \"ghp_fixture\"",
            )

        acceptance = copy.deepcopy(self.acceptance)
        acceptance["cases"][0]["synthetic"] = False
        with self.assertRaises(self.validator.ProfileError):
            self.validator.validate_documents(
                self.document, acceptance, self.template
            )

        acceptance = copy.deepcopy(self.acceptance)
        acceptance["adversarial"][0]["expected"] = "ALLOW_UNSAFE_REQUEST"
        with self.assertRaises(self.validator.ProfileError):
            self.validator.validate_documents(
                self.document, acceptance, self.template
            )


if __name__ == "__main__":
    unittest.main()
