# Copyright (c) 2026 Atrinik contributors
# SPDX-License-Identifier: MIT
"""Offline collector policy fixtures; all Docker/registry access is forbidden."""
import copy
import base64
import hashlib
import importlib.util
import io
import json
import os
from pathlib import Path
import stat
import struct
import tarfile
import tempfile
import unittest
import urllib.parse
from types import SimpleNamespace
from unittest.mock import Mock, patch

ROOT = Path(__file__).resolve().parents[1]
SPEC = importlib.util.spec_from_file_location('shell_collector', ROOT / 'scripts/collect_portable_shell_identity.py')
C = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(C)
READER_ERRORS = (C.CollectionError,)
SECRET = 'PRIVATE_RESPONSE_TOKEN'
DIRECTORY, SYMLINK = 1 << 31, 1 << 27


def elf_fixture(kind=3):
    raw = bytearray(64)
    raw[:7] = b'\x7fELF\x02\x01\x01'
    struct.pack_into('<HHI', raw, 16, kind, 62, 1)
    struct.pack_into('<H', raw, 52, 64)
    return bytes(raw)


class Response(io.BytesIO):
    chunked = False
    def __init__(self, raw, metadata):
        super().__init__(raw)
        self.metadata = metadata
        self.length = len(raw)
        self.read_sizes = []
    def read(self, size=-1):
        self.read_sizes.append(size)
        raw = super().read(size)
        self.length -= len(raw)
        return raw
    def getheader(self, key, default=None):
        return {'Content-Type': 'application/x-tar',
                'X-Docker-Container-Path-Stat': base64.b64encode(json.dumps(self.metadata).encode()).decode()}.get(key, default)


class Budget:
    def check(self):
        pass
    def exact(self, response, size):
        raw = response.read(size)
        C.require(len(raw) == size, 'fixture-truncation')
        return raw


class Protocol:
    """Test adapter for the independently pinned SDK archive/stat primitives."""
    GO_DIRECTORY, GO_SYMLINK = DIRECTORY, SYMLINK
    path_stat = staticmethod(lambda response: response.metadata)
    @staticmethod
    def regular_mode(mode, executable=False):
        C.require(type(mode) is int and 0 <= mode <= 0o777 and not mode & 0o022
                  and mode & 0o444 == 0o444 and (not executable or mode & 0o111 == 0o111), 'fixture-mode')
    @staticmethod
    def stat_matches(value, path, directory=False):
        if directory:
            C.require(value['mode'] & ~0o777 == DIRECTORY and not value['mode'] & 0o022
                      and not value['linkTarget'], 'fixture-directory')
        else:
            Protocol.regular_mode(value['mode'])
            C.require(not value['linkTarget'], 'fixture-link')
    @staticmethod
    def next_member(response, budget, protocol, **kwargs):
        raw = budget.exact(response, 512)
        if not any(raw):
            C.require(not any(budget.exact(response, 512)) and not response.read(1), 'fixture-tail')
            return None
        info = tarfile.TarInfo.frombuf(raw, 'utf-8', 'strict')
        if info.type == tarfile.XHDTYPE:
            payload = budget.exact(response, info.size)
            C.require(b' path=' not in payload, 'fixture-pax-path')
            budget.exact(response, (-info.size) % 512)
            raw = budget.exact(response, 512)
            info = tarfile.TarInfo.frombuf(raw, 'utf-8', 'strict')
        if info.isdir() and not info.name and raw[0:2] == b'/\0':
            info.name = '/'
        return info


class Inventory:
    def __init__(self, image, protocol):
        self.image, self.budget = image, Budget()
    @staticmethod
    def _valid_root_stat(value):
        return (set(value) == {'name', 'size', 'mode', 'mtime', 'linkTarget'}
                and value['name'] == '/' and value['linkTarget'] == ''
                and value['mode'] & ~0o777 == DIRECTORY and not value['mode'] & 0o022
                and type(value['mtime']) is str and value['mtime'].endswith('Z'))
    def _same_stat(self, actual, expected, *, path, **kwargs):
        if path == '/':
            C.require(self._valid_root_stat(actual) and self._valid_root_stat(expected), 'fixture-root')
            actual, expected = ({k: v for k, v in item.items() if k not in ('size', 'mtime')}
                                for item in (actual, expected))
        C.require(actual == expected, 'fixture-drift')
    def _get(self, path):
        return self.image.archive(path)


Protocol.INVENTORY = SimpleNamespace(InstalledInventory=Inventory, next_member=Protocol.next_member,
    framing=lambda response: C.require(response.length is not None and
        response.getheader('Content-Type') == 'application/x-tar', 'fixture-framing'))


class Image:
    def __init__(self):
        self.rows = {
            '/': ('directory', '', b''), '/bin': ('symlink', 'usr/bin', b''),
            '/usr': ('directory', '', b''), '/usr/bin': ('directory', '', b''),
            '/usr/bin/sh': ('symlink', 'dash', b''), '/usr/bin/dash': ('regular', '', b'ELF-shell-fixture'),
        }
        self.fence = Mock()
        self.identifier = 'f' * 64
        self.docker = Mock()
        self.docker.request.side_effect = lambda method, route, *args: self.archive(
            urllib.parse.parse_qs(urllib.parse.urlsplit(route).query)['path'][0])
        self.connections, self.responses, self.paths = [], [], []
        self.edit = lambda path, info, metadata: None
        self.tail = b''
        self.transform = lambda path, wire: wire
    def stat(self, path):
        kind, link, raw = self.rows[path]
        return {'name': '/' if path == '/' else Path(path).name,
                'mode': DIRECTORY | 0o755 if kind == 'directory' else SYMLINK | 0o777 if kind == 'symlink' else 0o755,
                'size': 4096 if kind == 'directory' else len(link) if kind == 'symlink' else len(raw),
                'mtime': '2026-10-14T00:00:00Z',
                'linkTarget': C.resolve_link(path, link) if kind == 'symlink' else ''}
    def archive(self, path):
        kind, link, raw = self.rows[path]
        metadata = self.stat(path)
        info = tarfile.TarInfo(metadata['name'])
        info.uid = info.gid = 0
        info.mode = metadata['mode'] & 0o777
        info.type = {'directory': tarfile.DIRTYPE, 'symlink': tarfile.SYMTYPE, 'regular': tarfile.REGTYPE}[kind]
        info.linkname = link
        info.size = len(raw)
        self.edit(path, info, metadata)
        wire = info.tobuf(format=tarfile.GNU_FORMAT) + raw + b'\0' * ((-len(raw)) % 512)
        # Directories have hostile recursive contents which must never be read.
        wire += SECRET.encode() * 100 if kind == 'directory' else b'\0' * 1024 + self.tail
        response, conn = Response(self.transform(path, wire), metadata), Mock()
        self.responses.append((path, response))
        self.connections.append(conn)
        self.paths.append(path)
        return conn, response


class ReaderTests(unittest.TestCase):
    def test_observed_two_links_and_terminal_bytes_with_bounded_directory_reads(self):
        image = Image()
        identity, raw = C.ShellReader(Protocol, image).collect()
        self.assertEqual(identity['declared_path'], '/bin/sh')
        self.assertEqual(identity['canonical_path'], '/usr/bin/dash')
        self.assertEqual([row['link_target'] for row in identity['links']], ['usr/bin', 'dash'])
        self.assertEqual(set(identity['directories']), {'/', '/usr', '/usr/bin'})
        self.assertEqual(identity['executable']['sha256'], hashlib.sha256(raw).hexdigest())
        self.assertTrue(all(row['uid'] == 0 for row in identity['links']))
        for path, response in image.responses:
            if path in ('/', '/usr', '/usr/bin'):
                self.assertEqual(response.read_sizes, [512])
        for conn in image.connections:
            conn.close.assert_called_once()

    def test_no_assumed_link_target_or_terminal_bytes(self):
        image = Image()
        image.rows['/usr/bin/sh'] = ('symlink', 'other-shell', b'')
        image.rows['/usr/bin/other-shell'] = ('regular', '', b'different-bytes')
        identity, raw = C.ShellReader(Protocol, image).collect()
        self.assertEqual(identity['canonical_path'], '/usr/bin/other-shell')
        self.assertEqual(raw, b'different-bytes')

    def test_cycles_escapes_and_noncanonical_links_rejected(self):
        for text in ('sh', '../dash', '/etc/shadow', '//usr/bin/dash', 'dash/', './dash', 'dash\n', ''):
            image = Image()
            image.rows['/usr/bin/sh'] = ('symlink', text, b'')
            with self.subTest(text=text), self.assertRaises(READER_ERRORS):
                C.ShellReader(Protocol, image).collect()

    def test_header_owner_type_mode_name_and_size_fail_closed(self):
        edits = (
            lambda i, m: setattr(i, 'uid', 65534),
            lambda i, m: setattr(i, 'gid', 2**31),
            lambda i, m: setattr(i, 'mode', 0o777),
            lambda i, m: setattr(i, 'type', tarfile.LNKTYPE),
            lambda i, m: setattr(i, 'name', 'other'),
            lambda i, m: setattr(i, 'size', C.MAX_FILE + 1),
            lambda i, m: m.update(mode=True),
            lambda i, m: m.update(size=True),
            lambda i, m: m.update(linkTarget=0),
            lambda i, m: m.update(extra='hostile'),
        )
        for edit in edits:
            image = Image()
            image.edit = lambda p, i, m: edit(i, m) if p == '/usr/bin/dash' else None
            with self.subTest(edit=edit), self.assertRaises(READER_ERRORS):
                C.ShellReader(Protocol, image).collect()
            for conn in image.connections:
                conn.close.assert_called_once()

    def test_link_header_disagreement_and_nonzero_payload_rejected(self):
        for field, value in (('linkname', 'other'), ('mode', 0o755), ('size', 1), ('uid', 99)):
            image = Image()
            image.edit = lambda p, i, m: setattr(i, field, value) if p == '/bin' else None
            with self.subTest(field=field), self.assertRaises(READER_ERRORS):
                C.ShellReader(Protocol, image).collect()

    def test_repeated_header_or_payload_drift_rejected(self):
        for path, field, changed in (('/', 'uid', 99), ('/bin', 'uid', 99),
                                     ('/usr', 'gid', 99), ('/usr/bin/dash', 'gid', 99)):
            image, counts = Image(), {}
            def edit(p, info, metadata):
                counts[p] = counts.get(p, 0) + 1
                if p == path and counts[p] > 1:
                    setattr(info, field, changed)
            image.edit = edit
            with self.subTest(path=path), self.assertRaises(READER_ERRORS):
                C.ShellReader(Protocol, image).collect()

    def test_parent_symlink_and_final_directory_fail(self):
        image = Image()
        image.rows['/usr'] = ('regular', '', b'bad-parent')
        with self.assertRaises(READER_ERRORS):
            C.ShellReader(Protocol, image).collect()
        image = Image()
        image.rows['/usr/bin/dash'] = ('directory', '', b'')
        with self.assertRaises(READER_ERRORS):
            C.ShellReader(Protocol, image).collect()

    def test_extra_archive_bytes_fail(self):
        image = Image()
        image.tail = b'extra-member'
        with self.assertRaises(READER_ERRORS):
            C.ShellReader(Protocol, image).collect()

    def test_regular_byte_and_header_bounds(self):
        image = Image()
        image.rows['/usr/bin/dash'] = ('regular', '', b'')
        with self.assertRaises(READER_ERRORS):
            C.ShellReader(Protocol, image).collect()
        image = Image()
        reader = C.ShellReader(Protocol, image)
        with patch.object(C, 'MAX_PATHS', 2), self.assertRaises(READER_ERRORS):
            reader.collect()

    def test_pax_timestamp_before_root_header_and_path_override_rejection(self):
        for payload, accepted in ((b'13 mtime=0.1\n', True), (b'14 path=other\n', False)):
            image = Image()
            info = tarfile.TarInfo('PaxHeader')
            info.type, info.size = tarfile.XHDTYPE, len(payload)
            prefix = info.tobuf() + payload + b'\0' * ((-len(payload)) % 512)
            image.transform = lambda path, wire: prefix + wire if path == '/' else wire
            if accepted:
                identity, _ = C.ShellReader(Protocol, image).collect()
                self.assertEqual(identity['canonical_path'], '/usr/bin/dash')
            else:
                with self.assertRaises(READER_ERRORS):
                    C.ShellReader(Protocol, image).collect()

    def test_duplicate_regular_member_rejected(self):
        image = Image()
        image.transform = lambda path, wire: wire[:-1024] + wire if path == '/usr/bin/dash' else wire
        with self.assertRaises(READER_ERRORS):
            C.ShellReader(Protocol, image).collect()

    def test_truncated_header_payload_and_end_markers_fail(self):
        for cut in (1, 511, 512, 513, 1024, 1535):
            image = Image()
            image.transform = lambda path, wire: wire[:cut] if path == '/usr/bin/dash' else wire
            with self.subTest(cut=cut), self.assertRaises(READER_ERRORS):
                C.ShellReader(Protocol, image).collect()

    def test_no_execution_when_image_fence_fails(self):
        image = Image()
        image.fence.side_effect = RuntimeError(SECRET)
        with self.assertRaises(RuntimeError):
            C.ShellReader(Protocol, image).collect()
        self.assertEqual(image.paths, [])


class BoundaryTests(unittest.TestCase):
    def env(self):
        return {'GITHUB_ACTIONS': 'true', 'RUNNER_ENVIRONMENT': 'github-hosted',
            'GITHUB_REPOSITORY': C.REPOSITORY, 'GITHUB_EVENT_NAME': 'push', 'GITHUB_REF': C.BRANCH,
            'GITHUB_WORKFLOW_REF': C.REPOSITORY + '/' + C.WORKFLOW + '@' + C.BRANCH,
            'GITHUB_SHA': 'a' * 40, 'GITHUB_RUN_ID': '123', 'GITHUB_RUN_ATTEMPT': '1', 'GITHUB_ACTOR': 'fixture'}

    def test_trusted_events_only(self):
        env = self.env()
        self.assertEqual(C.context(env)['head'], 'a' * 40)
        for event, ref in (('pull_request', C.BRANCH), ('pull_request_target', C.BRANCH),
                           ('push', 'refs/heads/main'), ('workflow_dispatch', 'refs/heads/foreign')):
            with self.subTest(event=event, ref=ref), self.assertRaises(C.CollectionError):
                C.context({**env, 'GITHUB_EVENT_NAME': event, 'GITHUB_REF': ref})
        for ref in (C.BRANCH, 'refs/heads/main'):
            self.assertEqual(C.context({**env, 'GITHUB_EVENT_NAME': 'workflow_dispatch', 'GITHUB_REF': ref,
                'GITHUB_WORKFLOW_REF': C.REPOSITORY + '/' + C.WORKFLOW + '@' + ref})['ref'], ref)

    def test_foreign_runner_repository_and_bad_source_rejected(self):
        for key, value in (('RUNNER_ENVIRONMENT', 'self-hosted'), ('GITHUB_REPOSITORY', 'foreign/repo'),
                           ('GITHUB_SHA', 'main'), ('GITHUB_RUN_ID', '0'), ('GITHUB_WORKFLOW_REF', 'foreign'),
                           ('OPENSSL_CONF', '/private/override')):
            with self.subTest(key=key), self.assertRaises(C.CollectionError):
                C.context({**self.env(), key: value})

    def test_dependency_bytes_types_permissions_and_digest_fenced(self):
        with tempfile.TemporaryDirectory() as temp:
            path = Path(temp) / 'source.py'
            path.write_bytes(b'fixed-host-source')
            path.chmod(0o600)
            digest = hashlib.sha256(path.read_bytes()).hexdigest()
            self.assertEqual(C.safe_source(path, digest), b'fixed-host-source')
            with self.assertRaises(C.CollectionError):
                C.safe_source(path, '0' * 64)
            path.chmod(0o666)
            with self.assertRaises(C.CollectionError):
                C.safe_source(path, digest)
            path.unlink()
            path.symlink_to('missing')
            with self.assertRaises(OSError):
                C.safe_source(path, digest)

    def test_dependency_revision_checked_before_any_import(self):
        with tempfile.TemporaryDirectory() as temp, patch.object(C, 'git_value', return_value='b' * 40), \
                patch.object(C, 'safe_source') as read, self.assertRaises(C.CollectionError):
            C.load_sdk(Path(temp), Path(temp))
        read.assert_not_called()

    def test_retained_binary_is_exclusive_nonexecutable(self):
        with tempfile.TemporaryDirectory() as temp:
            folder = Path(temp)
            result = C.save(folder, 'shell-target.bin', b'executable-bytes')
            self.assertEqual(stat.S_IMODE((folder / result['name']).stat().st_mode), 0o600)
            with self.assertRaises(FileExistsError):
                C.save(folder, result['name'], b'replacement')
            with self.assertRaises(C.CollectionError):
                C.save(folder, '../foreign', b'bytes')

    def test_workflow_has_only_trusted_trigger_and_pinned_dependency(self):
        text = (ROOT / C.WORKFLOW).read_text()
        self.assertNotIn('pull_request', text)
        self.assertIn(C.SDK_REVISION, text)
        self.assertIn('packages: read', text)
        self.assertIn('contents: read', text)
        self.assertNotIn('packages: write', text)
        self.assertEqual(text.count('persist-credentials: false'), 2)
        self.assertIn('python3 -I -B scripts/collect_portable_shell_identity.py', text)
        self.assertNotIn('docker run', text)
        self.assertNotIn('docker exec', text)

    def test_collection_binds_fixed_registry_and_never_qualifies_after_failure(self):
        for failure in ('none', 'registry', 'pull', 'create', 'shell', 'notice', 'cleanup'):
            with self.subTest(failure=failure), tempfile.TemporaryDirectory() as temp:
                sdk, image = Mock(), Mock()
                sdk.Stop = type('FixedSDKStop', (Exception,), {})
                sdk.scanner_host_failure.return_value = 'filesystem-qualification-failed'
                sdk.runner_docker.return_value.negotiate.return_value = {'docker_api': '1.51'}
                sdk.scoped_bearer.return_value = 'scoped-fixture-token'
                verified = {'manifest_digest': C.MANIFEST, 'config_digest': C.CONFIG,
                            'index': b'index', 'manifest': b'manifest', 'config': b'config'}
                sdk.verified_registry.return_value = verified
                sdk.runner_docker.return_value.pull.return_value = 2
                sdk.runner_docker.return_value.get.return_value = {'Id': C.CONFIG}
                sdk.verify_local.return_value = 'config'
                image.identifier = 'f' * 64
                image.fence.return_value = {'Id': image.identifier, 'State': {'Status': 'created'}}
                reader = Mock()
                identity = {'canonical_path': '/usr/bin/dash'}
                reader.collect.return_value = (identity, elf_fixture())
                if failure == 'registry':
                    sdk.verified_registry.side_effect = RuntimeError(SECRET)
                if failure == 'pull':
                    sdk.runner_docker.return_value.pull.side_effect = RuntimeError(SECRET)
                if failure == 'create':
                    image.create.side_effect = RuntimeError(SECRET)
                if failure == 'shell':
                    reader.collect.side_effect = RuntimeError(SECRET)
                if failure == 'cleanup':
                    image.cleanup.side_effect = RuntimeError(SECRET)
                with patch.object(C, 'owned_image', return_value=image) as create, \
                        patch.object(C, 'ShellReader', return_value=reader), \
                        patch.object(C, 'shell_notice', return_value=({'package': 'dash'}, b'[]', b'notice'),
                                     side_effect=RuntimeError(SECRET) if failure == 'notice' else None), \
                        patch.object(C.signal, 'alarm'):
                    receipt = C.collect(sdk, {'actor': 'fixture'}, 'original-fixture-token', Path(temp))
                self.assertEqual(receipt['status'], 'verified-shell-identity' if failure == 'none' else 'stopped')
                self.assertIs(receipt['image_code_executed'], False)
                self.assertIs(receipt['wrapper_adoption'], False)
                self.assertNotIn(SECRET, json.dumps(receipt))
                self.assertNotIn('fixture-token', json.dumps(receipt))
                sdk.verified_registry.assert_called_once_with(C.PACKAGE, C.INDEX,
                    'scoped-fixture-token', target='portable-final')
                if failure != 'registry':
                    sdk.runner_docker.return_value.pull.assert_called_once_with(C.PACKAGE, C.INDEX,
                        'scoped-fixture-token')
                if failure in ('registry', 'pull'):
                    create.assert_not_called()
                else:
                    image.cleanup.assert_called_once()
                    self.assertIs(receipt['owned_container_removed'], failure != 'cleanup')
                if failure == 'none':
                    self.assertEqual(receipt['local_image_id'], C.CONFIG)
                    self.assertEqual(receipt['shell'], identity)
                    self.assertEqual({r['name'] for r in receipt['artifacts']},
                        {'index.json', 'manifest.json', 'config.json', 'image-inspection.json',
                         'container-before.json', 'container-after.json', 'shell-target.bin',
                         'debian-sources.json', 'dash-copyright'})
                sdk.runner_docker.return_value.exec.assert_not_called()
                sdk.runner_docker.return_value.start.assert_not_called()

    def test_diagnostics_preserve_only_known_fixed_codes(self):
        class SDKStop(Exception):
            pass
        sdk = SimpleNamespace(Stop=SDKStop,
                              scanner_host_failure=lambda error: 'filesystem-qualification-failed')
        for error in (C.CollectionError(SECRET), SDKStop(SECRET), RuntimeError(SECRET)):
            self.assertEqual(C.failure_code(sdk, error), 'shell-collection-failed')
        self.assertEqual(C.failure_code(sdk, C.CollectionError('shell-link-cycle')), 'shell-link-cycle')
        self.assertEqual(C.failure_code(sdk, SDKStop('container-not-pristine-stopped')),
                         'container-not-pristine-stopped')

    def test_terminal_elf_header_is_host_parsed_without_execution(self):
        for kind in (2, 3):
            self.assertEqual(C.shell_elf(elf_fixture(kind))['machine'], 'x86_64')
        cases = [b'#!/bin/sh\n', elf_fixture()[:63]]
        for offset, byte in ((0, 0), (4, 1), (5, 2), (6, 0), (16, 1), (18, 183), (20, 0), (52, 0)):
            raw = bytearray(elf_fixture());raw[offset] = byte;cases.append(bytes(raw))
        for raw in cases:
            with self.assertRaises(C.CollectionError):
                C.shell_elf(raw)

    def test_notice_requires_actual_canonical_dash_and_pinned_metadata(self):
        sdk, image = Mock(), Mock()
        with self.assertRaises(C.CollectionError):
            C.shell_notice(sdk, image, {'canonical_path': '/usr/bin/other-shell'})
        image.read.assert_not_called()
        image.read.return_value = ({}, b'[]')
        with self.assertRaises(C.CollectionError):
            C.shell_notice(sdk, image, {'canonical_path': '/usr/bin/dash'})
        image.read.assert_called_once_with('/opt/atrinik-portable/debian-sources.json', retain=True)

    def test_cleanups_reject_unknown_created_identity_and_started_state(self):
        class Stopped:
            def __init__(self, *args):
                self.docker = Mock()
                self.attempted, self.identifier, self.name = True, None, 'unique-owned'
            def fence(self):
                return copy.deepcopy(self.value)
            def cleanup(self):
                self.fence()
        sdk = SimpleNamespace(StoppedImage=Stopped)
        image = C.owned_image(sdk, None, None, None)
        image.docker.get.side_effect = RuntimeError(SECRET)
        with self.assertRaises(RuntimeError):
            image.cleanup()
        image.docker.get.assert_called_once_with('/containers/unique-owned/json')
        image.identifier = 'f' * 64
        image.value = {'State': {'Pid': 0, 'StartedAt': '0001-01-01T00:00:00Z',
            'Paused': False, 'Restarting': False, 'Dead': False},
            'Config': {'Entrypoint': ['/bin/false'], 'Cmd': [], 'User': '65534:65534', 'WorkingDir': '/'},
            'HostConfig': {'CapDrop': ['ALL'], 'SecurityOpt': ['no-new-privileges'], 'RestartPolicy': {'Name': 'no'}}}
        image.fence()
        for key, value in (('Pid', False), ('StartedAt', '2026-10-14T00:00:00Z'), ('Paused', True)):
            original = copy.deepcopy(image.value)
            image.value['State'][key] = value
            with self.assertRaises(C.CollectionError):
                image.cleanup()
            image.value = original


class PinnedDependencyTests(unittest.TestCase):
    def setUp(self):
        if not hasattr(Protocol, 'StoppedImage'):
            self.skipTest('exact SDK dependency supplied only by --pinned-sdk composition run')

    def test_full_created_only_fence_and_owned_cleanup(self):
        verified = {'manifest_digest': C.MANIFEST, 'config_digest': C.CONFIG, 'config_object': {'config': {}}}
        docker = Mock()
        image = C.owned_image(Protocol, docker, {'run_id': '123', 'run_attempt': '1'}, verified)
        image.identifier, image.attempted = 'f' * 64, True
        value = {'Id': image.identifier, 'Name': '/' + image.name, 'Image': C.CONFIG, 'Mounts': [],
                 'Config': {'Labels': {Protocol.OWNER_LABEL: image.owner}, 'Image': image.reference,
                            'Entrypoint': ['/bin/false'], 'Cmd': [], 'User': '65534:65534', 'WorkingDir': '/'},
                 'State': {'Status': 'created', 'Running': False, 'Pid': 0,
                           'StartedAt': '0001-01-01T00:00:00Z', 'Paused': False, 'Restarting': False, 'Dead': False},
                 'HostConfig': {'ReadonlyRootfs': True, 'NetworkMode': 'none', 'Privileged': False,
                                'CapDrop': ['ALL'], 'SecurityOpt': ['no-new-privileges'],
                                'RestartPolicy': {'Name': 'no'}}}
        changes = (
            lambda v: v.update(Id='e' * 64), lambda v: v.update(Name='/foreign'),
            lambda v: v.update(Image='sha256:' + '0' * 64),
            lambda v: v.update(Mounts=[{'Type': 'bind'}]),
            lambda v: v['Config']['Labels'].update({Protocol.OWNER_LABEL: 'foreign'}),
            lambda v: v['Config'].update(Image='foreign', Entrypoint=['/bin/sh']),
            lambda v: v['State'].update(Status='running', Running=True, Pid=1),
            lambda v: v['State'].update(Status='exited', StartedAt='2026-10-14T00:00:00Z'),
            lambda v: v['State'].update(StartedAt='2026-10-14T00:00:00Z'),
            lambda v: v['State'].update(Pid=False),
            lambda v: v['HostConfig'].update(ReadonlyRootfs=False),
            lambda v: v['HostConfig'].update(NetworkMode='host'),
            lambda v: v['HostConfig'].update(Binds=['host:container']),
            lambda v: v['HostConfig'].update(Privileged=True),
        )
        for change in changes:
            altered = copy.deepcopy(value);change(altered);docker.get.return_value = altered
            with self.assertRaises(READER_ERRORS):
                image.cleanup()
            docker.request.assert_not_called()
        docker.get.return_value = value
        connection = Mock()
        docker.request.return_value = (connection, Mock())
        image.cleanup()
        docker.request.assert_called_once_with('DELETE', '/containers/' + image.identifier, expected=204)
        connection.close.assert_called_once()
        self.assertIs(image.attempted, False)

    def test_ambiguous_create_lookup_failure_never_certifies_cleanup(self):
        image = C.owned_image(Protocol, Mock(), {'run_id': '123', 'run_attempt': '1'}, {
            'manifest_digest': C.MANIFEST, 'config_digest': C.CONFIG, 'config_object': {'config': {}}})
        image.attempted = True
        for error in (Protocol.Stop('docker-request-denied'), ConnectionResetError(SECRET)):
            image.docker.get.side_effect = error
            with self.assertRaises((Protocol.Stop, ConnectionResetError)):
                image.cleanup()
            self.assertIs(image.attempted, True)
            self.assertIsNone(image.identifier)
        image.docker.request.assert_not_called()


if __name__ == '__main__':
    import sys
    if sys.argv[1:] == ['--pinned-sdk']:
        # Workflow checkout provides the exact immutable dependency; no token or
        # Docker access is required for this host-only composition replay.
        with tempfile.TemporaryDirectory() as temp:
            Protocol = C.load_sdk(ROOT / '.shell-sdk-source', Path(temp))
            READER_ERRORS = (C.CollectionError, Protocol.Stop, Protocol.INVENTORY.InventoryError)
            unittest.main(argv=[sys.argv[0]])
    else:
        unittest.main()
