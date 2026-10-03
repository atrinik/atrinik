# Copyright 2026 The Atrinik Project
# SPDX-License-Identifier: MIT
import copy
import importlib.util
import json
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location('build_corpus', ROOT / 'deploy/mcp/build_corpus.py')
corpus = importlib.util.module_from_spec(spec)
spec.loader.exec_module(corpus)


class CorpusTests(unittest.TestCase):
    def setUp(self):
        self.lock = json.loads((ROOT / 'deploy/mcp/source-lock.json').read_text())

    def test_exact_public_twenty_pins(self):
        rows = corpus.validate(self.lock)
        self.assertEqual(len(rows), 20)
        self.assertEqual(rows[0]['path'], '.')

    def test_untrusted_origins_paths_and_pins_rejected(self):
        for field, value in [('repository', 'https://user:secret@github.com/atrinik/client'),
                             ('repository', 'private/client'), ('path', '../secrets'),
                             ('commit', 'main'), ('commit', 'a' * 39), ('branch', 'other')]:
            lock = copy.deepcopy(self.lock)
            lock['checkouts'][0][field] = value
            with self.subTest(field=field, value=value), self.assertRaises(ValueError):
                corpus.validate(lock)
        self.lock['checkouts'].pop()
        with self.assertRaises(ValueError):
            corpus.validate(self.lock)

    def test_private_fields_and_duplicate_components_rejected(self):
        lock = copy.deepcopy(self.lock)
        lock['wrapper']['local_path'] = '/private/host'
        with self.assertRaises(ValueError):
            corpus.validate(lock)
        self.lock['checkouts'][0] = self.lock['checkouts'][1]
        with self.assertRaises(ValueError):
            corpus.validate(self.lock)

    def test_fetch_has_no_inherited_secrets_or_local_inputs(self):
        sha = 'a' * 40
        class Result:
            def __init__(self, value):
                self.stdout = value
        calls = []
        def run(command, **kwargs):
            calls.append((command, kwargs))
            return Result(sha.encode() if 'rev-parse' in command else b'')
        with tempfile.TemporaryDirectory() as name, patch.object(corpus.subprocess, 'run', side_effect=run), patch.dict('os.environ', {'SECRET_TOKEN': 'secret', 'GIT_CONFIG_GLOBAL': '/private'}):
            corpus.fetch(Path(name) / 'repo', 'atrinik/client', sha)
        for command, kwargs in calls:
            self.assertNotIn('SECRET_TOKEN', kwargs['env'])
            self.assertEqual(kwargs['env']['GIT_CONFIG_GLOBAL'], '/dev/null')
            self.assertEqual(kwargs['env']['GIT_TERMINAL_PROMPT'], '0')
            self.assertNotIn('/private', str(command))
        self.assertTrue(any('https://github.com/atrinik/client.git' in cmd for cmd, _ in calls))

    def test_existing_destination_is_never_reused(self):
        with tempfile.TemporaryDirectory() as name, patch.object(corpus, 'fetch') as fetch:
            with self.assertRaises(ValueError):
                corpus.build(self.lock, Path(name))
            fetch.assert_not_called()
