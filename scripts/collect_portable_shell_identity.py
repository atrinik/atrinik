#!/usr/bin/python3 -I
# Copyright (c) 2026 Atrinik contributors
# SPDX-License-Identifier: MIT
"""Hosted, created-only measurement of /bin/sh in the fixed released portable SDK.

Uses the unchanged MIT SDK collector at SDK_REVISION as a hash-bound host-only
implementation dependency. Its LICENSE and source notices stay with the staged
copy. No shell literal is adopted and no image executable is run by this tool.
"""
from collections import deque
import hashlib
import importlib.util
import json
import os
from pathlib import Path, PurePosixPath
import re
import resource
import signal
import stat
import struct
import subprocess
import sys
import tarfile
import tempfile
import time
import urllib.parse

SDK_REVISION = '4b68b6869ac7f6d0fcb2478777bf0dbf528797a0'
SDK_TREE = '36b57d4d864066b533d05162c4bdc53c8027b18d'
DEPENDENCIES = {
    'tools/collect-released-sdk-metadata.py': '31de7f6acc41b8c2428baf06dfb792cfd8eed1f374cb01d1a63997eef6a66605',
    'tools/released_sdk_inventory.py': '1259fe9e59b18d946703bcf88b13df8c176ceec74ccc319656fd4a2ff60026ac',
    'LICENSE': '6fd130923f519654060f651202b7de71545c24ec3047ae687fa5249290456a51',
}
PACKAGE = 'atrinik/classic-portable-build'
INDEX = 'sha256:74761e930f5d57face2d81efb0dc50dd4ce32ce1204810e56580bf68a5972ebc'
MANIFEST = 'sha256:5e30957e54277bdaadeeffa42246258dc9df2086f3fb41f2125d041735ac72aa'
CONFIG = 'sha256:c35dbeac2679d79dab524a13c9a3b74a8187878ecd3b3dff52bd2892418d7a79'
PRODUCER = 'eb1f6af78216adb4375826cdb70ef78a61ab403a'
CONSUMER = 'bd5317a03cdc82f029105c5dc26428b09fcd54b3'
REPOSITORY = 'atrinik/atrinik'
BRANCH = 'refs/heads/build/llvm-portable-shell-identity'
WORKFLOW = '.github/workflows/linux-portable-shell-identity.yml'
MAX_FILE = 4 * 1024 * 1024
MAX_PATHS = 32


class CollectionError(Exception):
    """Only fixed local codes enter diagnostics."""


def require(condition, reason):
    if not condition:
        raise CollectionError(reason)


SHELL_FAILURE_CODES = frozenset({
    'artifact-limit',
    'checkout-invalid',
    'checkout-mismatch',
    'cleanup-identity-invalid',
    'collection-interrupted',
    'container-config-mismatch',
    'container-ever-started-or-ambiguous',
    'context-invalid',
    'dependency-revision-mismatch',
    'dependency-source-mismatch',
    'dependency-source-unsafe',
    'dependency-target-mismatch',
    'event-not-allowed',
    'hosted-runner-required',
    'invocation-invalid',
    'registry-coordinate-mismatch',
    'repository-mismatch',
    'shell-debian-metadata-invalid',
    'shell-debian-metadata-mismatch',
    'shell-directory-header',
    'shell-extra-member',
    'shell-elf-header-invalid',
    'shell-header-or-bytes-drift',
    'shell-header-owner-or-name',
    'shell-link-cycle',
    'shell-link-invalid',
    'shell-link-limit',
    'shell-link-outside-namespace',
    'shell-notice-association-invalid',
    'shell-notice-association-unsupported',
    'shell-notice-size',
    'shell-padding',
    'shell-parent-not-directory',
    'shell-path-limit',
    'shell-regular-header',
    'shell-regular-mode',
    'shell-resolution-invalid',
    'shell-stat-invalid',
    'shell-stat-name',
    'shell-stat-schema',
    'shell-stat-time-invalid',
    'shell-symlink-header',
    'shell-symlink-stat',
    'shell-terminal-not-regular',
    'tls-override-not-allowed',
    'token-unavailable',
    'workflow-mismatch',
})
SDK_FAILURE_CODES = frozenset({
    'ambiguous-linux-amd64-manifest',
    'archive-metadata-mismatch',
    'archive-owner-mismatch',
    'blob-redirect-denied',
    'config-platform-mismatch',
    'config-size-mismatch',
    'container-isolation-mismatch',
    'container-not-pristine-stopped',
    'container-ownership-mismatch',
    'daemon-proxy-not-allowed',
    'docker-endpoint-override-not-allowed',
    'docker-platform-mismatch',
    'invalid-config-rootfs',
    'invalid-image-manifest',
    'invalid-oci-index',
    'invalid-path-stat',
    'invalid-pax-header',
    'invalid-regular-file',
    'invalid-scoped-token',
    'invalid-token-response',
    'local-runner-socket-required',
    'manifest-size-mismatch',
    'missing-path-stat',
    'path-stat-name-mismatch',
    'producer-label-mismatch',
    'token-redirect-denied',
    'tool-not-executable',
    'truncated-archive',
    'unexpected-archive-entry',
    'unexpected-archive-tail',
    'unsafe-docker-socket',
    'unsafe-file-permissions',
    'unsafe-image-directory',
    'unsafe-pax-header',
    'unsupported-docker-api',
})


def failure_code(sdk, error):
    if (type(error) is CollectionError and len(error.args) == 1 and type(error.args[0]) is str
            and error.args[0] in SHELL_FAILURE_CODES):
        return error.args[0]
    if (type(error) is sdk.Stop and len(error.args) == 1 and type(error.args[0]) is str
            and error.args[0] in SDK_FAILURE_CODES):
        return error.args[0]
    code = sdk.scanner_host_failure(error)
    return code if code != 'filesystem-qualification-failed' else 'shell-collection-failed'


def canonical(value):
    return (json.dumps(value, sort_keys=True, indent=2) + '\n').encode()


def context(env):
    require(sys.platform == 'linux' and env.get('GITHUB_ACTIONS') == 'true'
            and env.get('RUNNER_ENVIRONMENT') == 'github-hosted', 'hosted-runner-required')
    require(env.get('GITHUB_REPOSITORY') == REPOSITORY, 'repository-mismatch')
    event, ref = env.get('GITHUB_EVENT_NAME'), env.get('GITHUB_REF')
    require((event == 'push' and ref == BRANCH) or
            (event == 'workflow_dispatch' and ref in (BRANCH, 'refs/heads/main')), 'event-not-allowed')
    require(env.get('GITHUB_WORKFLOW_REF') == REPOSITORY + '/' + WORKFLOW + '@' + ref,
            'workflow-mismatch')
    result = {'repository': REPOSITORY, 'workflow': WORKFLOW, 'event': event, 'ref': ref}
    for key, pattern, field in (
        ('GITHUB_SHA', r'[0-9a-f]{40}', 'head'),
        ('GITHUB_RUN_ID', r'[1-9][0-9]{0,19}', 'run_id'),
        ('GITHUB_RUN_ATTEMPT', r'[1-9][0-9]{0,19}', 'run_attempt'),
        ('GITHUB_ACTOR', r'[A-Za-z0-9][A-Za-z0-9-]{0,38}(?:\[bot\])?', 'actor'),
    ):
        value = env.get(key, '')
        require(re.fullmatch(pattern, value) is not None, 'context-invalid')
        result[field] = value
    require(not any(env.get(key) for key in ('OPENSSL_CONF', 'OPENSSL_MODULES', 'SSLKEYLOGFILE')),
            'tls-override-not-allowed')
    return result


def git_value(root, expression):
    result = subprocess.run(['/usr/bin/git', '-C', str(root), 'rev-parse', expression],
        env={'PATH': '/usr/bin:/bin', 'GIT_CONFIG_NOSYSTEM': '1', 'GIT_CONFIG_GLOBAL': '/dev/null'},
        stdin=subprocess.DEVNULL, stdout=subprocess.PIPE, stderr=subprocess.DEVNULL, timeout=15)
    require(result.returncode == 0 and len(result.stdout) == 41, 'checkout-invalid')
    return result.stdout.decode('ascii').strip()


def safe_source(path, expected):
    fd = os.open(path, os.O_RDONLY | os.O_NOFOLLOW | os.O_NONBLOCK)
    try:
        before = os.fstat(fd)
        require(stat.S_ISREG(before.st_mode) and before.st_uid == os.getuid()
                and not before.st_mode & 0o022 and 0 < before.st_size <= 1024 * 1024,
                'dependency-source-unsafe')
        raw = os.read(fd, before.st_size + 1)
        def identity(item):
            return (item.st_dev, item.st_ino, item.st_mode, item.st_uid, item.st_gid,
                    item.st_size, item.st_mtime_ns, item.st_ctime_ns)
        require(len(raw) == before.st_size and identity(os.fstat(fd)) == identity(before)
                and identity(path.stat(follow_symlinks=False)) == identity(before)
                and hashlib.sha256(raw).hexdigest() == expected, 'dependency-source-mismatch')
        return raw
    finally:
        os.close(fd)


def load_sdk(root, staging):
    require(git_value(root, 'HEAD') == SDK_REVISION and git_value(root, 'HEAD^{tree}') == SDK_TREE,
            'dependency-revision-mismatch')
    # Freeze the entire non-stdlib import closure and its MIT notice before any
    # import. The exclusive private staging directory has no checkout races.
    for name, expected in DEPENDENCIES.items():
        raw = safe_source(root / name, expected)
        destination = staging / Path(name).name
        fd = os.open(destination, os.O_WRONLY | os.O_CREAT | os.O_EXCL, 0o400)
        with os.fdopen(fd, 'wb') as stream:
            stream.write(raw)
    spec = importlib.util.spec_from_file_location('fixed_sdk_shell_host',
        staging / 'collect-released-sdk-metadata.py')
    sdk = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(sdk)
    require(sdk.TARGETS['portable-final'] == (PACKAGE, '1.22.0', INDEX)
            and sdk.FINAL_TARGETS['portable-final'] == {
                'producer_revision': PRODUCER, 'consumer_commit': CONSUMER,
                'manifest_digest': MANIFEST, 'config_digest': CONFIG}, 'dependency-target-mismatch')
    return sdk


def owned_image(sdk, docker, run, verified):
    class CreatedShellImage(sdk.StoppedImage):
        def fence(self):
            value = super().fence()
            state, cfg, host = value['State'], value['Config'], value['HostConfig']
            require(type(state.get('Pid')) is int and state['Pid'] == 0
                    and state.get('StartedAt') == '0001-01-01T00:00:00Z'
                    and all(state.get(key) is False for key in ('Paused', 'Restarting', 'Dead')),
                    'container-ever-started-or-ambiguous')
            require(cfg.get('Entrypoint') == ['/bin/false'] and cfg.get('Cmd') in (None, [])
                    and cfg.get('User') == '65534:65534' and cfg.get('WorkingDir') == '/'
                    and host.get('CapDrop') == ['ALL']
                    and host.get('SecurityOpt') == ['no-new-privileges']
                    and host.get('RestartPolicy', {}).get('Name') == 'no', 'container-config-mismatch')
            return value

        def cleanup(self):
            if self.attempted and self.identifier is None:
                # A lost create response plus 404 cannot rule out pending create.
                # Reuse ordinary exact-name lookup, which rejects every non-200.
                value = self.docker.get('/containers/' + self.name + '/json')
                identifier = value.get('Id')
                require(type(identifier) is str and re.fullmatch(r'[0-9a-f]{64}', identifier),
                        'cleanup-identity-invalid')
                self.identifier = identifier
            super().cleanup()
    return CreatedShellImage(docker, 'portable-final', run, verified)


def allowed_path(path):
    return (path in ('/', '/bin', '/usr', '/usr/bin') or
            re.fullmatch(r'/(?:usr/)?bin/[a-zA-Z0-9_+.-]{1,64}', path) is not None)


def resolve_link(path, text):
    require(type(text) is str and 0 < len(text) <= 256 and
            re.fullmatch(r'/?[A-Za-z0-9_+.-]+(?:/[A-Za-z0-9_+.-]+)*', text)
            and not any(part in ('.', '..') for part in text.split('/')), 'shell-link-invalid')
    resolved = text if text.startswith('/') else str(PurePosixPath(path).parent / text)
    require(allowed_path(resolved), 'shell-link-outside-namespace')
    return resolved


class ShellReader:
    def __init__(self, sdk, image):
        self.sdk, self.image = sdk, image
        self.inventory = sdk.INVENTORY.InstalledInventory(image, sdk)
        self.budget = self.inventory.budget
        self.budget.deadline = time.monotonic() + 120
        self.stats, self.records, self.raw_files = {}, {}, {}

    def read(self, path):
        require(allowed_path(path) and len(self.records) < MAX_PATHS, 'shell-path-limit')
        self.image.fence()
        before = self.image.stat(path)
        if path in self.stats:
            self.inventory._same_stat(before, self.stats[path], site='shell-repeat', path=path)
        require(type(before) is dict and set(before) == {'name', 'size', 'mode', 'mtime', 'linkTarget'},
                'shell-stat-schema')
        require(type(before['mode']) is int and type(before['size']) is int
                and type(before['linkTarget']) is str
                and 0 <= before['size'] <= MAX_FILE, 'shell-stat-invalid')
        # Reuse the reviewed exact-root schema/time validation, including the
        # documented root-only size/mtime exception at the recheck below.
        probe = {**before, 'name': '/', 'mode': self.sdk.GO_DIRECTORY | 0o755, 'linkTarget': ''}
        require(self.inventory._valid_root_stat(probe), 'shell-stat-time-invalid')
        require(before['name'] == ('/' if path == '/' else PurePosixPath(path).name), 'shell-stat-name')
        conn, response = self.inventory._get(path)
        try:
            self.inventory._same_stat(self.sdk.path_stat(response), before, site='shell-header', path=path)
            self.sdk.INVENTORY.framing(response)
            info = self.sdk.INVENTORY.next_member(response, self.budget, self.sdk, symlink=True)
            require(info is not None and info.name == before['name'] and info.uid == 0
                    and type(info.gid) is int and 0 <= info.gid <= 2**31 - 1,
                    'shell-header-owner-or-name')
            record = {'path': path, 'uid': info.uid, 'gid': info.gid, 'mode': info.mode}
            if info.type == tarfile.DIRTYPE:
                self.sdk.stat_matches({**before, 'name': ''} if path == '/' else before, path, directory=True)
                require(info.size == 0 and info.mode == before['mode'] & 0o777, 'shell-directory-header')
                record['type'] = 'directory'
                # Close after the first bounded header; never read / or /usr recursively.
            else:
                if info.type == tarfile.SYMTYPE:
                    require(before['mode'] == self.sdk.GO_SYMLINK | 0o777 and info.mode == 0o777
                            and info.size == 0, 'shell-symlink-header')
                    target = resolve_link(path, info.linkname)
                    require(before['linkTarget'] == target and before['size'] == len(info.linkname.encode()),
                            'shell-symlink-stat')
                    record.update(type='symlink', link_target=info.linkname)
                else:
                    require(info.type in (tarfile.REGTYPE, tarfile.AREGTYPE) and not info.linkname
                            and 0 < info.size == before['size'] <= MAX_FILE, 'shell-regular-header')
                    self.sdk.stat_matches(before, path)
                    self.sdk.regular_mode(info.mode, executable=True)
                    require(info.mode == before['mode'], 'shell-regular-mode')
                    raw = self.budget.exact(response, info.size)
                    require(not any(self.budget.exact(response, (-info.size) % 512)), 'shell-padding')
                    record.update(type='regular', size=len(raw), sha256=hashlib.sha256(raw).hexdigest())
                    self.raw_files[path] = raw
                require(self.sdk.INVENTORY.next_member(response, self.budget, self.sdk, symlink=True) is None,
                        'shell-extra-member')
            self.budget.check()
        finally:
            conn.close()
        if path in self.records:
            require(record == self.records[path], 'shell-header-or-bytes-drift')
        self.stats[path], self.records[path] = before, record
        return record

    def collect(self):
        pending, current, links = deque(['bin', 'sh']), '/', []
        self.read('/')
        for _ in range(MAX_PATHS):
            require(pending, 'shell-resolution-invalid')
            path = str(PurePosixPath(current) / pending.popleft())
            record = self.read(path)
            if record['type'] == 'symlink':
                require(path not in links, 'shell-link-cycle')
                links.append(path)
                target = resolve_link(path, record['link_target'])
                pending = deque(PurePosixPath(target).parts[1:]) + pending
                current = '/'
            elif pending:
                require(record['type'] == 'directory', 'shell-parent-not-directory')
                current = path
            else:
                require(record['type'] == 'regular', 'shell-terminal-not-regular')
                break
        else:
            raise CollectionError('shell-link-limit')
        # Full nonroot stats and stable root fields are rechecked. Re-read every
        # bounded directory/symlink header as HEAD does not expose ownership.
        for observed_path, expected in list(self.stats.items()):
            self.inventory._same_stat(self.image.stat(observed_path), expected,
                                      site='shell-recheck', path=observed_path)
            saved = self.records[observed_path]
            require(self.read(observed_path) == saved, 'shell-header-or-bytes-drift')
        self.image.fence()
        return {'schema_version': 1, 'declared_path': '/bin/sh', 'canonical_path': path,
                'links': [self.records[name] for name in links],
                'directories': {name: item for name, item in self.records.items() if item['type'] == 'directory'},
                'executable': self.records[path]}, self.raw_files[path]


def save(folder, name, raw):
    require('/' not in name and len(raw) <= 16 * 1024 * 1024, 'artifact-limit')
    fd = os.open(folder / name, os.O_WRONLY | os.O_CREAT | os.O_EXCL | os.O_NOFOLLOW, 0o600)
    with os.fdopen(fd, 'wb') as stream:
        stream.write(raw)
    return {'name': name, 'size': len(raw), 'sha256': hashlib.sha256(raw).hexdigest()}


def shell_elf(raw):
    # Host-only header decoding. The fixed OS loader/shared libraries stay in
    # the existing trusted boundary; an unexpected script needs another proof.
    require(len(raw) >= 64 and raw[:7] == b'\x7fELF\x02\x01\x01'
            and struct.unpack_from('<HHI', raw, 16) in ((2, 62, 1), (3, 62, 1))
            and struct.unpack_from('<H', raw, 52)[0] == 64, 'shell-elf-header-invalid')
    return {'class': 64, 'byte_order': 'little', 'machine': 'x86_64',
            'type': 'ET_EXEC' if struct.unpack_from('<H', raw, 16)[0] == 2 else 'ET_DYN'}


def shell_notice(sdk, image, identity):
    """Retain the producer's actual Debian association/notice for measured dash."""
    require(identity['canonical_path'] == '/usr/bin/dash', 'shell-notice-association-unsupported')
    metadata_path = '/opt/atrinik-portable/debian-sources.json'
    _, metadata = image.read(metadata_path, retain=True)
    require(hashlib.sha256(metadata).hexdigest() ==
            '0882c128d989bf4b8491d7ed666b73f9d572a4da201caa155ee711cff5c9bf13',
            'shell-debian-metadata-mismatch')
    rows = sdk.decode(metadata)
    require(type(rows) is list, 'shell-debian-metadata-invalid')
    selected = [row for row in rows if type(row) is dict and row.get('package') == 'dash']
    require(len(selected) == 1 and selected[0].get('source') == 'dash'
            and selected[0].get('notice_directory') == '/usr/share/doc/dash', 'shell-notice-association-invalid')
    path = '/usr/share/doc/dash/copyright'
    reader = sdk.INVENTORY.InstalledInventory(image, sdk)
    image.fence()
    observations = reader._parents(path, protected=True)
    expected = image.stat(path)
    sdk.stat_matches(expected, path)
    require(0 < expected['size'] <= MAX_FILE, 'shell-notice-size')
    observations[path] = expected
    conn, response = reader._get(path)
    try:
        reader._same_stat(sdk.path_stat(response), expected, site='shell-notice', path=path)
        _, notice = sdk.single_file_archive(response, path, expected, retain=True, root_owned=True)
    finally:
        conn.close()
    reader._recheck(observations)
    return selected[0], metadata, notice


def collect(sdk, run, token, folder):
    receipt = {**run, 'schema_version': 1, 'claim': 'fixed-sdk-shell-installed-identity',
        'image_code_executed': False, 'wrapper_adoption': False, 'index_digest': INDEX,
        'manifest_digest': MANIFEST, 'config_digest': CONFIG, 'producer_revision': PRODUCER,
        'consumer_commit': CONSUMER, 'sdk_revision': SDK_REVISION, 'sdk_tree': SDK_TREE,
        'dependencies': DEPENDENCIES, 'status': 'started', 'artifacts': []}
    image, stage = None, 'registry'
    try:
        docker = sdk.runner_docker()
        receipt.update(docker.negotiate())
        bearer = sdk.scoped_bearer(PACKAGE, token, run['actor'])
        verified = sdk.verified_registry(PACKAGE, INDEX, bearer, target='portable-final')
        require(verified['manifest_digest'] == MANIFEST and verified['config_digest'] == CONFIG,
                'registry-coordinate-mismatch')
        for name in ('index', 'manifest', 'config'):
            receipt['artifacts'].append(save(folder, name + '.json', verified[name]))
        stage = 'pull'
        receipt['pull_stream_records'] = docker.pull(PACKAGE, INDEX, bearer)
        del bearer
        stage = 'image-inspection'
        reference = 'ghcr.io/' + PACKAGE + '@' + INDEX
        query = urllib.parse.urlencode({'platform': json.dumps({'os': 'linux', 'architecture': 'amd64'})})
        local = docker.get('/images/' + urllib.parse.quote(reference, safe='') + '/json?' + query)
        kind = sdk.verify_local(local, reference, INDEX, verified)
        receipt.update(local_image_id=local['Id'], local_image_identity_kind=kind)
        receipt['artifacts'].append(save(folder, 'image-inspection.json', canonical(local)))
        stage = 'container-create'
        image = owned_image(sdk, docker, run, verified)
        image.create()
        receipt['container_id'] = image.identifier
        receipt['artifacts'].append(save(folder, 'container-before.json', canonical(image.fence())))
        stage = 'shell-chain'
        identity, raw = ShellReader(sdk, image).collect()
        receipt['shell_elf'] = shell_elf(raw)
        receipt['shell'] = identity
        stage = 'shell-notice'
        association, metadata, notice = shell_notice(sdk, image, identity)
        receipt['debian_source_association'] = association
        receipt['artifacts'].append(save(folder, 'debian-sources.json', metadata))
        receipt['artifacts'].append(save(folder, 'dash-copyright', notice))
        receipt['artifacts'].append(save(folder, 'shell-target.bin', raw))
        receipt['artifacts'].append(save(folder, 'container-after.json', canonical(image.fence())))
        receipt['status'] = 'verified-shell-identity'
    except BaseException as error:
        reason = failure_code(sdk, error)
        receipt.update(status='stopped', failure_stage=stage, reason=reason,
                       failure={'stage': stage, 'reason': reason})
    finally:
        if image is not None:
            try:
                signal.alarm(120)
                image.cleanup()
                receipt['owned_container_removed'] = True
            except BaseException as error:
                receipt.update(status='stopped', reason='owned-cleanup-failed', owned_container_removed=False,
                               cleanup_failure={'reason': failure_code(sdk, error)})
    return receipt


def main():
    token = os.environ.pop('GHCR_TOKEN', '')
    for key in ('GH_TOKEN', 'GITHUB_TOKEN', 'GH_CONFIG_DIR', 'XDG_CONFIG_HOME', 'GH_DEBUG'):
        os.environ.pop(key, None)
    def interrupted(_signum, _frame):
        raise CollectionError('collection-interrupted')
    folder = None
    try:
        require(len(sys.argv) == 1 and sys.flags.isolated == 1 and sys.flags.dont_write_bytecode == 1,
                'invocation-invalid')
        os.umask(0o077)
        resource.setrlimit(resource.RLIMIT_CORE, (0, 0))
        for number in (signal.SIGALRM, signal.SIGTERM, signal.SIGINT):
            signal.signal(number, interrupted)
        signal.alarm(1800)
        run = context(os.environ)
        root = Path(__file__).resolve().parents[1]
        require(Path.cwd().resolve() == root and Path(os.environ['GITHUB_WORKSPACE']).resolve() == root
                and git_value(root, 'HEAD') == run['head'], 'checkout-mismatch')
        require(0 < len(token) < 16384 and all(32 < ord(c) < 127 for c in token), 'token-unavailable')
        temporary = Path(os.environ['RUNNER_TEMP']).resolve(strict=True)
        folder = temporary / 'portable-shell-identity'
        folder.mkdir(mode=0o700)
        with tempfile.TemporaryDirectory(prefix='portable-shell-dependency-', dir=temporary) as private:
            sdk = load_sdk(root / '.shell-sdk-source', Path(private))
            receipt = collect(sdk, run, token, folder)
        receipt['collector_sha256'] = hashlib.sha256(Path(__file__).read_bytes()).hexdigest()
        record = save(folder, 'receipt.json', canonical(receipt))
        save(folder, 'receipt.sha256', (record['sha256'] + '  receipt.json\n').encode())
        print(json.dumps({'status': receipt['status'], 'receipt_sha256': record['sha256']}))
        return 0 if receipt['status'] == 'verified-shell-identity' else 1
    except BaseException:
        print('{"status":"stopped","reason":"shell-collector-preflight-failed"}')
        return 1
    finally:
        signal.alarm(0)


if __name__ == '__main__':
    raise SystemExit(main())
