from __future__ import annotations

import unittest

from atrinik_workspace.supervisor import ServerReadinessCapture, _initial_status


class ServerReadinessCaptureTests(unittest.TestCase):
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
