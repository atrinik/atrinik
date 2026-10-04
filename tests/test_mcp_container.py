# Copyright 2026 The Atrinik Project
# SPDX-License-Identifier: MIT
import io
import os
from pathlib import Path
import stat
import tempfile
import unittest
from unittest.mock import patch

from atrinik_workspace import mcp_container


class ContainerTests(unittest.TestCase):
    def test_private_token_creation_and_idempotence(self):
        with tempfile.TemporaryDirectory() as name:
            root = Path(name)
            mcp_container.init_token(root)
            token = root / 'token'
            value = token.read_bytes()
            self.assertEqual(len(value), 65)
            self.assertEqual(stat.S_IMODE(token.stat().st_mode), 0o600)
            mcp_container.init_token(root)
            self.assertEqual(token.read_bytes(), value)

    def test_unsafe_directory_and_existing_tokens_are_rejected(self):
        with tempfile.TemporaryDirectory() as name:
            root = Path(name)
            root.chmod(0o755)
            with self.assertRaises(ValueError):
                mcp_container.init_token(root)
            root.chmod(0o700)
            token = root / 'token'
            token.write_text('do not overwrite')
            token.chmod(0o600)
            with self.assertRaises(ValueError):
                mcp_container.init_token(root)
            self.assertEqual(token.read_text(), 'do not overwrite')
            token.unlink()
            token.symlink_to('/dev/null')
            with self.assertRaises(OSError):
                mcp_container.init_token(root)
            token.unlink()
            os.mkfifo(token, 0o600)
            with self.assertRaises(ValueError):
                mcp_container.init_token(root)

    def test_token_permissions_and_hardlinks_fail(self):
        with tempfile.TemporaryDirectory() as name:
            root = Path(name)
            mcp_container.init_token(root)
            token = root / 'token'
            token.chmod(0o640)
            with self.assertRaises(ValueError):
                mcp_container.init_token(root)
            token.chmod(0o600)
            os.link(token, root / 'alias')
            with self.assertRaises(ValueError):
                mcp_container.init_token(root)

    def test_http_defaults_and_explicit_root(self):
        with patch.object(mcp_container.mcp_http, 'main', return_value=0) as run:
            self.assertEqual(mcp_container.main(['http', '--public-domain', 'mcp.example.com']), 0)
            self.assertEqual(run.call_args.args[0], ['--root', '/opt/atrinik-corpus', '--allowed-host', 'mcp.example.com', '--allowed-origin', 'https://mcp.example.com'])
            mcp_container.main(['http', '--root=/explicit'])
            self.assertEqual(run.call_args.args[0], ['--root=/explicit'])

    def test_domain_injection_fails_before_http_start(self):
        for value in ('a', 'a.com\n{', 'a.com:443', '*.com', '-a.com', 'a..com', 'A.com', 'a.com/$X'):
            with self.subTest(value=value), self.assertRaises(ValueError):
                mcp_container.public_domain(value)
        with patch.object(mcp_container.mcp_http, 'main') as run, patch('sys.stderr', io.StringIO()):
            self.assertEqual(mcp_container.main(['http', '--public-domain', 'x\n{}']), 1)
            run.assert_not_called()
