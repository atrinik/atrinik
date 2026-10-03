# Copyright 2026 The Atrinik Project
# SPDX-License-Identifier: MIT
"""Fetch only the allowlisted public corpus at full immutable commit IDs.

No host repository, Git configuration, credentials, or working copy is input.
Pins must belong to the declared upstream branch; complete history is retained
for opt-in provenance queries.
"""
from __future__ import annotations

import argparse
import json
from pathlib import Path
import re
import subprocess

NAMES = frozenset('client server protocol editor renderer content-toolkit website content sound resources metaserver-worker devcontainer github-settings observatory deploy-control web-platform classic playtester tools'.split())
SHA = re.compile(r'[0-9a-f]{40}\Z')


def validate(lock):
    if set(lock) != {'schema_version', 'wrapper', 'checkouts'} or lock['schema_version'] != 1:
        raise ValueError('invalid source lock')
    rows = lock['checkouts']
    if not isinstance(rows, list) or len(rows) != len(NAMES):
        raise ValueError('incomplete source lock')
    seen = set()
    for row in rows:
        if set(row) != {'repository', 'path', 'branch', 'commit'}:
            raise ValueError('unexpected source lock fields')
        name = row['path']
        if name not in NAMES or name in seen or row['repository'] != 'atrinik/' + name:
            raise ValueError('unsafe source origin or destination')
        if row['branch'] != 'main' or not isinstance(row['commit'], str) or not SHA.fullmatch(row['commit']):
            raise ValueError('full source commit required')
        seen.add(name)
    wrapper = lock['wrapper']
    if (set(wrapper) != {'repository', 'branch', 'commit'} or wrapper['repository'] != 'atrinik/atrinik'
            or wrapper['branch'] != 'main' or not isinstance(wrapper['commit'], str)
            or not SHA.fullmatch(wrapper['commit'])):
        raise ValueError('invalid wrapper corpus pin')
    return [dict(wrapper, path='.'), *rows]


def fetch(destination, repository, commit, *, branch='main'):
    if (repository not in {'atrinik/' + name for name in NAMES | {'atrinik'}}
            or branch != 'main' or not isinstance(commit, str) or not SHA.fullmatch(commit)):
        raise ValueError('invalid public source pin')
    # Construct the entire environment; inherited proxies, URL rewrites, credential
    # helpers, templates, hooks, alternate object stores and LFS are never used.
    environment = {'PATH': '/usr/bin:/bin', 'HOME': '/nonexistent', 'LC_ALL': 'C',
                   'GIT_CONFIG_NOSYSTEM': '1', 'GIT_CONFIG_GLOBAL': '/dev/null',
                   'GIT_TERMINAL_PROMPT': '0', 'GIT_LFS_SKIP_SMUDGE': '1'}
    destination.mkdir(parents=True, exist_ok=False)
    def git(*args):
        return subprocess.run(['git', '-c', 'credential.helper=', '-c', 'core.hooksPath=/dev/null',
                               '-c', 'protocol.allow=never', '-c', 'protocol.https.allow=always',
                               '-C', str(destination), *args], env=environment,
                              check=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE,
                              timeout=600).stdout.decode().strip()
    git('init', '--initial-branch=main', '--template=')
    git('remote', 'add', 'origin', 'https://github.com/' + repository + '.git')
    upstream = 'refs/remotes/origin/' + branch
    git('fetch', '--no-tags', 'origin', 'refs/heads/' + branch + ':' + upstream)
    if git('rev-parse', '--is-shallow-repository') != 'false':
        raise ValueError('complete source history required')
    # A raw SHA fetch followed by a local branch label cannot prove membership.
    # Fetch the actual declared upstream ref, then verify the immutable pin
    # before assigning any local branch to it (older ancestors remain valid).
    if git('rev-parse', '--verify', commit + '^{commit}') != commit:
        raise ValueError('source pin must identify a commit')
    git('merge-base', '--is-ancestor', commit, upstream)
    git('checkout', '-B', branch, commit)
    if git('rev-parse', 'HEAD') != commit or git('status', '--porcelain', '--untracked-files=no'):
        raise ValueError('source pin mismatch')
    # The generated Git metadata is public-only. Remove transport-only residue.
    (destination / '.git' / 'FETCH_HEAD').unlink(missing_ok=True)


def build(lock, destination):
    rows = validate(lock)
    if destination.exists():
        raise ValueError('corpus destination must be absent')
    for row in rows:
        fetch(destination / row['path'], row['repository'], row['commit'], branch=row['branch'])


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--lock', type=Path, required=True)
    parser.add_argument('--destination', type=Path, required=True)
    args = parser.parse_args()
    build(json.loads(args.lock.read_text()), args.destination)


if __name__ == '__main__':
    main()
