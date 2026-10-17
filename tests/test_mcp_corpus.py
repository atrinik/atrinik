# Copyright 2026 The Atrinik Project
# SPDX-License-Identifier: MIT
import copy
import importlib.util
import json
from pathlib import Path
import subprocess
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
            if '--is-shallow-repository' in command:
                return Result(b'false')
            return Result(sha.encode() if 'rev-parse' in command else b'')
        with tempfile.TemporaryDirectory() as name, patch.object(corpus.subprocess, 'run', side_effect=run), patch.dict('os.environ', {'SECRET_TOKEN': 'secret', 'GIT_CONFIG_GLOBAL': '/private'}):
            corpus.fetch(Path(name) / 'repo', 'atrinik/client', sha)
        for command, kwargs in calls:
            self.assertNotIn('SECRET_TOKEN', kwargs['env'])
            self.assertEqual(kwargs['env']['GIT_CONFIG_GLOBAL'], '/dev/null')
            self.assertEqual(kwargs['env']['GIT_TERMINAL_PROMPT'], '0')
            self.assertNotIn('/private', str(command))
        self.assertTrue(any('https://github.com/atrinik/client.git' in cmd for cmd, _ in calls))

    def git(self, repository, *arguments):
        return subprocess.run(
            ['git', '-C', str(repository), *arguments], check=True,
            capture_output=True, text=True,
        ).stdout.strip()

    def upstream(self, root):
        repository = root / 'upstream'
        repository.mkdir()
        self.git(repository, 'init', '-q', '-b', 'main')
        self.git(repository, 'config', 'user.name', 'First Fixture Author')
        self.git(repository, 'config', 'user.email', 'fixture@example.invalid')
        self.git(repository, 'config', 'commit.gpgsign', 'false')
        source = repository / 'source.txt'
        source.write_text('original line\n')
        self.git(repository, 'add', '.')
        self.git(repository, 'commit', '-qm', 'original contribution')
        first = self.git(repository, 'rev-parse', 'HEAD')
        self.git(repository, 'config', 'user.name', 'Second Fixture Author')
        source.write_text('original line\nsecond line\n')
        self.git(repository, 'commit', '-qam', 'second contribution')
        pin = self.git(repository, 'rev-parse', 'HEAD')
        self.git(repository, 'commit', '--allow-empty', '-qm', 'newer branch tip')
        return repository, first, pin

    def fixture_transport(self, upstream, *, preload=None, make_shallow=False):
        real_run = subprocess.run
        calls = []

        def run(command, **kwargs):
            calls.append(command)
            if 'fetch' in command:
                # Only the network transport is replaced. All initialization,
                # ref resolution, ancestry checks, and checkout use real Git.
                self.assertIn('refs/heads/main:refs/remotes/origin/main', command)
                self.assertNotIn('--depth=1', command)
                prefix = command[:command.index('fetch')]
                prefix += ['-c', 'protocol.file.allow=always']
                if preload:
                    real_run(prefix + ['fetch', '--no-tags', upstream.as_uri(), preload], **kwargs)
                result = real_run(prefix + ['fetch', '--no-tags', upstream.as_uri(),
                                           'refs/heads/main:refs/remotes/origin/main'], **kwargs)
                if make_shallow:
                    destination = Path(command[command.index('-C') + 1])
                    tip = real_run(prefix + ['rev-parse', 'refs/remotes/origin/main'], **kwargs).stdout
                    (destination / '.git/shallow').write_bytes(tip)
                return result
            return real_run(command, **kwargs)

        return patch.object(corpus.subprocess, 'run', side_effect=run), calls

    def test_older_upstream_member_keeps_full_history_and_original_attribution(self):
        with tempfile.TemporaryDirectory() as name:
            root = Path(name)
            upstream, first, pin = self.upstream(root)
            destination = root / 'corpus'
            transport, _calls = self.fixture_transport(upstream)
            with transport:
                corpus.fetch(destination, 'atrinik/client', pin)
            self.assertEqual(self.git(destination, 'rev-parse', 'HEAD'), pin)
            self.assertEqual(self.git(destination, 'branch', '--show-current'), 'main')
            self.assertEqual(self.git(destination, 'rev-parse', '--is-shallow-repository'), 'false')
            self.assertEqual(self.git(destination, 'rev-list', '--count', 'HEAD'), '2')
            blame = self.git(destination, 'blame', '--line-porcelain', '-L', '1,1', 'HEAD', '--', 'source.txt')
            self.assertTrue(blame.startswith(first + ' '))
            self.assertIn('author First Fixture Author\n', blame)
            self.assertFalse((destination / '.git/FETCH_HEAD').exists())

    def test_unrelated_available_commit_is_not_relabelled_as_main(self):
        with tempfile.TemporaryDirectory() as name:
            root = Path(name)
            upstream, first, _pin = self.upstream(root)
            self.git(upstream, 'checkout', '-qb', 'other', first)
            self.git(upstream, 'commit', '--allow-empty', '-qm', 'unrelated contribution')
            unrelated = self.git(upstream, 'rev-parse', 'HEAD')
            destination = root / 'corpus'
            transport, calls = self.fixture_transport(upstream, preload='refs/heads/other')
            with transport, self.assertRaises(subprocess.CalledProcessError):
                corpus.fetch(destination, 'atrinik/client', unrelated)
            self.assertTrue(any('merge-base' in command for command in calls))
            self.assertFalse(any('checkout' in command for command in calls))

    def test_missing_declared_branch_cannot_be_replaced_by_raw_sha(self):
        with tempfile.TemporaryDirectory() as name:
            root = Path(name)
            upstream, _first, pin = self.upstream(root)
            self.git(upstream, 'branch', '-m', 'other')
            transport, calls = self.fixture_transport(upstream)
            with transport, self.assertRaises(subprocess.CalledProcessError):
                corpus.fetch(root / 'corpus', 'atrinik/client', pin)
            self.assertFalse(any('checkout' in command for command in calls))

    def test_shallow_fetch_is_rejected_before_labelling(self):
        with tempfile.TemporaryDirectory() as name:
            root = Path(name)
            upstream, _first, pin = self.upstream(root)
            transport, calls = self.fixture_transport(upstream, make_shallow=True)
            with transport, self.assertRaisesRegex(ValueError, 'complete source history'):
                corpus.fetch(root / 'corpus', 'atrinik/client', pin)
            self.assertFalse(any('checkout' in command for command in calls))

    def test_tag_object_cannot_stand_in_for_exact_commit_pin(self):
        with tempfile.TemporaryDirectory() as name:
            root = Path(name)
            upstream, _first, pin = self.upstream(root)
            self.git(upstream, '-c', 'tag.gpgsign=false', 'tag', '-am', 'fixture tag', 'fixture', pin)
            tag = self.git(upstream, 'rev-parse', 'refs/tags/fixture')
            transport, calls = self.fixture_transport(upstream, preload='refs/tags/fixture')
            with transport, self.assertRaisesRegex(ValueError, 'identify a commit'):
                corpus.fetch(root / 'corpus', 'atrinik/client', tag)
            self.assertFalse(any('checkout' in command for command in calls))

    def test_fetch_does_not_reuse_existing_destination(self):
        with tempfile.TemporaryDirectory() as name, patch.object(corpus.subprocess, 'run') as run:
            with self.assertRaises(FileExistsError):
                corpus.fetch(Path(name), 'atrinik/client', 'a' * 40)
            run.assert_not_called()

    def test_fetch_validates_origin_branch_and_commit_before_git(self):
        with tempfile.TemporaryDirectory() as name, patch.object(corpus.subprocess, 'run') as run:
            for repository, branch, commit in [
                ('private/client', 'main', 'a' * 40),
                ('atrinik/client', 'other', 'a' * 40),
                ('atrinik/client', 'main', 'HEAD'),
            ]:
                with self.subTest(repository=repository, branch=branch, commit=commit):
                    with self.assertRaises(ValueError):
                        corpus.fetch(Path(name) / 'repo', repository, commit, branch=branch)
            run.assert_not_called()

    def test_existing_destination_is_never_reused(self):
        with tempfile.TemporaryDirectory() as name, patch.object(corpus, 'fetch') as fetch:
            with self.assertRaises(ValueError):
                corpus.build(self.lock, Path(name))
            fetch.assert_not_called()
