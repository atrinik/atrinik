# Copyright 2026 The Atrinik Project
# SPDX-License-Identifier: MIT
"""Bounded offline-container wire acceptance; no model, credentials or host mounts."""
import argparse
import json
import os
import selectors
import subprocess
import time
import uuid


def smoke(image):
    name = 'atrinik-mcp-smoke-' + uuid.uuid4().hex[:12]
    process = subprocess.Popen(['docker', 'run', '--name', name, '--rm', '-i', '--read-only',
                                '--network', 'none', '--cap-drop', 'ALL', '--security-opt',
                                'no-new-privileges', '--user', '10001:10001', image, 'stdio'],
                               stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.DEVNULL)
    reader = selectors.DefaultSelector()
    reader.register(process.stdout, selectors.EVENT_READ)
    counter = 0
    def send(method, params, notification=False):
        nonlocal counter
        counter += 1
        request = {'jsonrpc': '2.0', 'method': method, 'params': params}
        if not notification:
            request['id'] = counter
        process.stdin.write(json.dumps(request).encode() + b'\n')
        process.stdin.flush()
        if notification:
            return None
        deadline = time.monotonic() + 30
        data = bytearray()
        while not data.endswith(b'\n'):
            if time.monotonic() > deadline or not reader.select(max(0, deadline-time.monotonic())):
                raise RuntimeError('MCP response timeout')
            byte = os.read(process.stdout.fileno(), 1)
            if not byte or len(data) > 65536:
                raise RuntimeError('MCP response missing or exceeds bound')
            data.extend(byte)
        response = json.loads(data)
        if response.get('id') != counter or 'error' in response:
            raise RuntimeError('MCP request failed: ' + method)
        result = response['result']
        if result.get('isError'):
            raise RuntimeError('MCP tool failed: ' + method)
        return result
    try:
        initialized = send('initialize', {'protocolVersion':'2025-11-25', 'capabilities':{},
                                         'clientInfo':{'name':'atrinik-image-smoke','version':'1'}})
        assert initialized['protocolVersion'] == '2025-11-25'
        send('notifications/initialized', {}, True)
        tools = send('tools/list', {})['tools']
        assert len(tools) == 7 and any(tool['name'] == 'context_guidance' for tool in tools)
        send('resources/list', {})
        catalog = send('tools/call', {'name':'context_describe', 'arguments':{'profile':'default'}})
        assert catalog['structuredContent']['data']['items']
        guidance = send('tools/call', {'name':'context_guidance','arguments':{}})
        uri = guidance['structuredContent']['data']['resources'][0]['uri']
        contents = send('resources/read', {'uri':uri})['contents']
        assert contents and contents[0]['text']
        # Prove both generations' component objects are available without network.
        for profile, component in [('default','client'), ('classic','classic-server')]:
            send('tools/call', {'name':'context_resolve','arguments':{'profile':profile,'component':component}})
        process.stdin.close()
        if process.wait(timeout=10) != 0:
            raise RuntimeError('MCP exit failed')
    finally:
        reader.close()
        if process.poll() is None:
            subprocess.run(['docker','rm','-f', name], stdout=subprocess.DEVNULL,
                           stderr=subprocess.DEVNULL, timeout=15, check=False)
            process.kill()
            process.wait(timeout=5)
    print('PASS: offline read-only UID10001 stdio lifecycle, seven tools, catalog, guidance, resources, both profiles, clean EOF')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--image', required=True)
    smoke(parser.parse_args().image)
