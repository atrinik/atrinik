# Copyright 2026 The Atrinik Project
# SPDX-License-Identifier: MIT
"""Docker Compose's own parser verifies the remote deployment boundary."""
import json
import os
from pathlib import Path
import shutil
import subprocess
import unittest

ROOT = Path(__file__).resolve().parents[1]
COMPOSE = ROOT / 'deploy/mcp/compose.yaml'
IMAGE = 'ghcr.io/atrinik/atrinik-mcp@sha256:' + 'a' * 64


@unittest.skipUnless(shutil.which('docker'), 'Docker CLI unavailable')
class MCPComposeTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        probe = subprocess.run(['docker', 'compose', 'version'], capture_output=True)
        if probe.returncode:
            raise unittest.SkipTest('Docker Compose unavailable')

    def config(self, domain='mcp.example.com', image=IMAGE):
        env = {key: value for key, value in os.environ.items()
               if not key.startswith(('MCP_', 'COMPOSE_'))}
        if domain is not None:
            env['MCP_DOMAIN'] = domain
        if image is not None:
            env['MCP_IMAGE'] = image
        return subprocess.run(['docker', 'compose', '--env-file', '/dev/null',
                               '-f', str(COMPOSE), 'config', '--format', 'json'],
                              env=env, capture_output=True, text=True, timeout=20)

    def test_required_settings_fail_closed(self):
        for domain, image in [(None, IMAGE), ('', IMAGE), ('mcp.example.com', None),
                              ('mcp.example.com', '')]:
            with self.subTest(domain=domain, image=image):
                self.assertNotEqual(self.config(domain, image).returncode, 0)

    def test_transport_and_volume_boundaries(self):
        result = self.config()
        self.assertEqual(result.returncode, 0, result.stderr)
        config = json.loads(result.stdout)
        init, mcp, proxy = (config['services'][key] for key in ['init-token', 'mcp', 'proxy'])
        self.assertEqual(init['image'], mcp['image'])
        for service in (init, mcp):
            self.assertEqual(service['user'], '10001:10001')
            self.assertTrue(service['read_only'])
            self.assertEqual(service['cap_drop'], ['ALL'])
            self.assertIn('no-new-privileges:true', service['security_opt'])
            self.assertEqual(float(service['cpus']), 2)
            self.assertEqual(service['mem_limit'], '268435456')
            self.assertEqual(service['pids_limit'], 128)
            self.assertFalse(service.get('ports'))
            self.assertTrue(all(v['type'] == 'volume' for v in service['volumes']))
        self.assertEqual(init['network_mode'], 'none')
        self.assertEqual(list(mcp['networks']), ['backend'])
        self.assertTrue(config['networks']['backend']['internal'])
        self.assertEqual(mcp['depends_on']['init-token']['condition'], 'service_completed_successfully')
        self.assertEqual(proxy['depends_on']['mcp']['condition'], 'service_healthy')
        self.assertTrue(mcp['volumes'][0]['read_only'])
        self.assertFalse(init['volumes'][0].get('read_only', False))
        self.assertEqual({p['target'] for p in proxy['ports']}, {80, 443})
        self.assertRegex(proxy['image'], r'^caddy:[^@]+@sha256:[a-f0-9]{64}$')
        self.assertEqual(mcp['command'][-2:], ['--public-domain', 'mcp.example.com'])
        self.assertEqual(mcp['healthcheck']['test'][-2:], ['--public-domain', 'mcp.example.com'])
        self.assertNotIn('token', str(mcp.get('environment', {})))
        self.assertNotIn('Authorization', result.stdout)

    def test_domain_is_never_interpreted_by_a_shell(self):
        # Runtime's public-domain validator must reject these. Compose must pass
        # them as one argv element, without expanding commands or Caddy syntax.
        for domain in ['$(id).example.com', 'x.example.com\n{\n}', 'x:443', '${HOME}.example.com']:
            with self.subTest(domain=domain):
                result = self.config(domain)
                self.assertEqual(result.returncode, 0, result.stderr)
                mcp = json.loads(result.stdout)['services']['mcp']
                self.assertEqual(mcp['command'][-1].replace('$$', '$'), domain)
                self.assertEqual(mcp['healthcheck']['test'][0], 'CMD')
                self.assertEqual(mcp['healthcheck']['test'][-1].replace('$$', '$'), domain)


if __name__ == '__main__':
    unittest.main()
