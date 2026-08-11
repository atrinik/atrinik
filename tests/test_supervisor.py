from __future__ import annotations

import json
from pathlib import Path
import tempfile
import unittest
from unittest import mock

from atrinik_workspace import supervisor as supervisor_module
from atrinik_workspace.supervisor import (
    ServerReadinessCapture,
    _initial_status,
    supervise,
)


class ServerReadinessCaptureTests(unittest.TestCase):
    def test_unidentified_service_is_registered_for_bounded_cleanup(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            spec = {
                "name": "cleanup",
                "profile": "default",
                "dependencies": ["client"],
                "state": None,
                "build_root": "/tmp/build",
                "resolved": {},
                "endpoint": None,
                "services": {
                    "client": {
                        "command": ["client"],
                        "cwd": str(root),
                        "log": str(root / "client.log"),
                    }
                },
            }
            spec_path = root / "spec.json"
            spec_path.write_text(json.dumps(spec), encoding="utf-8")
            process = mock.MagicMock(pid=1234)

            with (
                mock.patch.object(
                    supervisor_module,
                    "process_start_time",
                    side_effect=["1", None],
                ),
                mock.patch.object(
                    supervisor_module.subprocess, "Popen", return_value=process
                ),
                mock.patch.object(supervisor_module, "terminate") as terminate,
                mock.patch.object(supervisor_module.os, "close") as close,
            ):
                self.assertEqual(supervise(spec_path, None, 7, 8), 1)

            terminate.assert_called_once()
            self.assertEqual(terminate.call_args.args[0], {"client": process})
            self.assertEqual(
                {call.args[0] for call in close.call_args_list}, {7, 8}
            )

    def test_requires_fingerprint_and_finished_server_startup(self) -> None:
        capture = ServerReadinessCapture()

        capture.feed(b"QUIC certificate SHA-256: " + b"A" * 64 + b"\n")

        self.assertEqual(capture.fingerprint, "a" * 64)
        self.assertFalse(capture.event.is_set())

        capture.feed(b"Server ready. Waiting for connections...\n")

        self.assertTrue(capture.event.is_set())

    def test_recognizes_readiness_messages_split_across_chunks(self) -> None:
        capture = ServerReadinessCapture()

        capture.feed(b"Server ready. Waiting for connec")
        capture.feed(b"tions...\nQUIC certificate SHA-256: " + b"b" * 31)
        capture.feed(b"b" * 33 + b"\n")

        self.assertEqual(capture.fingerprint, "b" * 64)
        self.assertTrue(capture.event.is_set())

    def test_historical_spec_remains_identifiably_historical(self) -> None:
        spec = {
            "name": "historical",
            "profile": "classic",
            "dependencies": ["server"],
            "state": "/tmp/state",
            "build_root": "/tmp/build",
            "resolved": {},
            "endpoint": None,
        }

        historical = _initial_status(spec, "123")

        self.assertNotIn("stack", historical)
        self.assertNotIn("providers", historical)

        current = _initial_status(
            {
                **spec,
                "stack": "classic",
                "providers": {"server": "classic-server"},
            },
            "123",
        )
        self.assertEqual(current["stack"], "classic")
        self.assertEqual(current["providers"], {"server": "classic-server"})

        with self.assertRaisesRegex(RuntimeError, "identity is incomplete"):
            _initial_status({**spec, "stack": "classic"}, "123")


if __name__ == "__main__":
    unittest.main()
