# Copyright 2026 The Atrinik Project
# SPDX-License-Identifier: MIT
"""Self-contained public source image entrypoint."""
from __future__ import annotations

import argparse
import http.client
import re
import os
from pathlib import Path
import secrets
import stat
import sys

from atrinik_workspace.mcp_context import ContextService
from atrinik_workspace.mcp_contract import ContractError
from atrinik_workspace.mcp_server import ContextServer
from atrinik_workspace import mcp_http, mcp_legacy

CORPUS = Path('/opt/atrinik-corpus')
AUTH = Path('/var/lib/atrinik-auth')


def init_token(directory=AUTH):
    """Initialize only an exclusive private volume; never print its secret."""
    fd = os.open(directory, os.O_RDONLY | os.O_DIRECTORY | os.O_NOFOLLOW)
    try:
        info = os.fstat(fd)
        if info.st_uid != os.getuid() or stat.S_IMODE(info.st_mode) != 0o700:
            raise ValueError('unsafe authentication directory')
        try:
            token_fd = os.open('token', os.O_WRONLY | os.O_CREAT | os.O_EXCL | os.O_NOFOLLOW,
                               0o600, dir_fd=fd)
        except FileExistsError:
            token_fd = os.open('token', os.O_RDONLY | os.O_NOFOLLOW | os.O_NONBLOCK, dir_fd=fd)
            try:
                info = os.fstat(token_fd)
                if (not stat.S_ISREG(info.st_mode) or info.st_uid != os.getuid()
                        or stat.S_IMODE(info.st_mode) != 0o600 or info.st_nlink != 1):
                    raise ValueError('unsafe existing token')
                value = os.read(token_fd, 258)
                if len(value) != 65 or value[-1:] != b'\n' or any(
                        byte not in b'abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789-_' for byte in value[:-1]):
                    raise ValueError('unrecognized existing token')
            finally:
                os.close(token_fd)
            return
        try:
            with os.fdopen(token_fd, 'wb') as output:
                output.write(secrets.token_urlsafe(48).encode('ascii') + b'\n')
                output.flush()
                os.fsync(output.fileno())
            os.fsync(fd)
        except BaseException:
            os.unlink('token', dir_fd=fd)
            raise
    finally:
        os.close(fd)


def public_domain(value):
    if (len(value) > 253 or '.' not in value or not all(
            re.fullmatch(r'[a-z0-9](?:[a-z0-9-]{0,61}[a-z0-9])?', label)
            for label in value.split('.'))):
        raise ValueError('invalid public domain')
    return value


def healthcheck(domain, port=8765):
    domain = public_domain(domain)
    connection = http.client.HTTPConnection('127.0.0.1', port, timeout=5)
    try:
        connection.request('GET', '/healthz', headers={'Host': domain, 'Origin': 'https://' + domain})
        response = connection.getresponse()
        return 0 if response.status == 200 and response.read(64) == b'{"status":"ok"}' else 1
    finally:
        connection.close()


def main(argv=None):
    arguments = list(sys.argv[1:] if argv is None else argv)
    mode = arguments.pop(0) if arguments else 'stdio'
    try:
        if mode == 'http':
            domain_parser = argparse.ArgumentParser(add_help=False)
            domain_parser.add_argument('--public-domain')
            domain_args, arguments = domain_parser.parse_known_args(arguments)
            if domain_args.public_domain is not None:
                domain = public_domain(domain_args.public_domain)
                arguments += ['--allowed-host', domain, '--allowed-origin', 'https://' + domain]
            if '--root' not in arguments and not any(arg.startswith('--root=') for arg in arguments):
                arguments = ['--root', str(CORPUS), *arguments]
            return mcp_http.main(arguments)
        parser = argparse.ArgumentParser(description=__doc__)
        if mode == 'stdio':
            parser.add_argument('--root', type=Path, default=CORPUS)
            args = parser.parse_args(arguments)
            return mcp_legacy.run(ContextServer(ContextService(args.root)), sys.stdin.buffer, sys.stdout.buffer)
        if mode == 'healthcheck':
            parser.add_argument('--public-domain', required=True)
            parser.add_argument('--port', type=int, default=8765)
            args = parser.parse_args(arguments)
            return healthcheck(args.public_domain, args.port)
        if mode == 'init-token':
            parser.parse_args(arguments)
            init_token()
            return 0
        parser.error('command must be stdio, http, healthcheck, or init-token')
    except (OSError, ValueError, ContractError):
        print('atrinik-mcp: invalid or unavailable configuration', file=sys.stderr)
        return 1
    return 2


if __name__ == '__main__':
    raise SystemExit(main())
