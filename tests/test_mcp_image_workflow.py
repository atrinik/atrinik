# Copyright 2026 The Atrinik Project
# SPDX-License-Identifier: MIT
from pathlib import Path
import re
import unittest


ROOT = Path(__file__).resolve().parents[1]
WORKFLOW = ROOT / ".github" / "workflows" / "mcp-image.yml"


class McpImageWorkflowTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.workflow = WORKFLOW.read_text(encoding="utf-8")

    def test_package_write_is_confined_to_the_publish_job(self) -> None:
        self.assertEqual(self.workflow.count("packages: write"), 1)
        publish = self.workflow.split("\n  publish:\n", 1)[1]
        self.assertIn("permissions:\n      contents: read\n      packages: write", publish)
        self.assertNotIn("pull_request_target", self.workflow)

    def test_manual_publication_requires_an_exact_allowed_branch_tip(self) -> None:
        self.assertIn("workflow_dispatch:", self.workflow)
        self.assertIn("^[0-9a-f]{40}$", self.workflow)
        self.assertIn('git ls-remote --exit-code origin "refs/heads/$TARGET_BRANCH"', self.workflow)
        self.assertIn('$REQUESTED_REVISION != "$remote_revision"', self.workflow)
        self.assertIn("needs: [select-source, validate]", self.workflow)
        publish = self.workflow.split("\n  publish:\n", 1)[1]
        self.assertEqual(
            publish.count('git ls-remote --exit-code origin "refs/heads/$TARGET_BRANCH"'),
            1,
        )

    def test_actions_and_publication_identity_are_immutable(self) -> None:
        uses = re.findall(r"uses: ([^\s]+)", self.workflow)
        self.assertTrue(uses)
        for action in uses:
            with self.subTest(action=action):
                self.assertRegex(action, r"^[^@]+@[0-9a-f]{40}$")
        self.assertIn("SOURCE_REVISION=${{ needs.select-source.outputs.revision }}", self.workflow)
        self.assertEqual(
            self.workflow.count('test -z "$(git status --porcelain --untracked-files=all)"'),
            2,
        )
        self.assertIn("org.opencontainers.image.revision=", self.workflow)
        self.assertIn("org.opencontainers.image.source=", self.workflow)
        self.assertIn("ghcr.io/atrinik/atrinik-mcp:sha-", self.workflow)
        self.assertNotRegex(self.workflow, r"atrinik-mcp:(?:latest|main)(?:\s|$)")
        self.assertIn("^sha256:[0-9a-f]{64}$", self.workflow)

    def test_pull_requests_build_and_smoke_without_publishing(self) -> None:
        validate = self.workflow.split("\n  validate:\n", 1)[1].split(
            "\n  publish:\n", 1
        )[0]
        self.assertIn("push: false", validate)
        self.assertIn("deploy/mcp/official.Dockerfile", validate)
        self.assertIn("python3 deploy/mcp/smoke_stdio.py", validate)

    def test_publish_smokes_the_exact_local_image_before_authentication_and_push(self) -> None:
        publish = self.workflow.split("\n  publish:\n", 1)[1]
        local = "atrinik-mcp:publish-${{ needs.select-source.outputs.revision }}"
        build = publish.index(f"tags: {local}")
        smoke = publish.index(f"--image {local}")
        login = publish.index("uses: docker/login-action@")
        push = publish.index('docker push "$image"')
        self.assertLess(build, smoke)
        self.assertLess(smoke, login)
        self.assertLess(login, push)
        self.assertNotIn("push: true", publish)


if __name__ == "__main__":
    unittest.main()
