# Copyright 2026 The Atrinik Project
# SPDX-License-Identifier: MIT
"""Minimal verified-TLS container health check; no authentication or source reads."""
import argparse
import http.client
from pathlib import Path
import ssl


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--ca-file", type=Path, required=True)
    parser.add_argument("--port", type=int, default=8765)
    args = parser.parse_args(argv)
    connection = None
    try:
        context = ssl.create_default_context(cafile=str(args.ca_file))
        connection = http.client.HTTPSConnection("localhost", args.port, timeout=5, context=context)
        connection.request("GET", "/healthz")
        response = connection.getresponse()
        return 0 if response.status == 200 and response.read(64) == b'{"status":"ok"}' else 1
    except (OSError, ValueError, http.client.HTTPException):
        return 1
    finally:
        if connection is not None:
            connection.close()


if __name__ == "__main__":
    raise SystemExit(main())
