#!/usr/bin/env python3
"""Isolated end-to-end automation for Atrinik's direct UDP/QUIC stack."""

from __future__ import annotations

import argparse
import ipaddress
import os
from pathlib import Path
import selectors
import socket
import stat
import struct
import subprocess
import tempfile
import threading
import unittest


TIMEOUT_SECONDS = 12
PAYLOAD = "atrinik-quic-e2e"
SCENARIOS = (
    "identity",
    "quic",
    "streams",
    "disconnect",
    "stun",
    "punch",
    "mapping",
)


class NativeDriver:
    def __init__(self, path: Path, verbose: bool) -> None:
        self.path = path
        self.verbose = verbose

    def command(
        self,
        *arguments: object,
        check: bool = True,
        timeout: int = TIMEOUT_SECONDS,
    ) -> subprocess.CompletedProcess[str]:
        command = [str(self.path), *(str(argument) for argument in arguments)]
        result = subprocess.run(
            command,
            check=False,
            capture_output=True,
            text=True,
            timeout=timeout,
        )
        if self.verbose or (check and result.returncode != 0):
            print(f"$ {' '.join(command)}")
            if result.stdout:
                print(result.stdout, end="")
            if result.stderr:
                print(result.stderr, end="")
        if check and result.returncode != 0:
            raise AssertionError(
                f"driver exited {result.returncode}: {result.stderr.strip()}"
            )
        return result

    @staticmethod
    def marker(output: str, prefix: str) -> str:
        matches = [line for line in output.splitlines() if line.startswith(prefix)]
        if len(matches) != 1:
            raise AssertionError(
                f"expected one {prefix!r} marker, got {matches!r} in {output!r}"
            )
        return matches[0].removeprefix(prefix)

    def server(
        self, identity: Path, mode: str = "server"
    ) -> tuple[subprocess.Popen[str], int, str]:
        port = 0
        command = [str(self.path), mode, str(port), str(identity)]
        if mode == "server":
            command.append(PAYLOAD)
        process = subprocess.Popen(
            command,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True,
        )
        assert process.stdout is not None
        selector = selectors.DefaultSelector()
        selector.register(process.stdout, selectors.EVENT_READ)
        events = selector.select(TIMEOUT_SECONDS)
        selector.close()
        if not events:
            process.terminate()
            stdout, stderr = process.communicate(timeout=TIMEOUT_SECONDS)
            raise AssertionError(
                f"server did not become ready; stdout={stdout!r}, stderr={stderr!r}"
            )
        ready = ""
        for _ in range(16):
            line = process.stdout.readline().strip()
            if line.startswith("READY "):
                ready = line
                break
        if not ready:
            process.terminate()
            stdout, stderr = process.communicate(timeout=TIMEOUT_SECONDS)
            raise AssertionError(
                f"unexpected server readiness {ready!r}; "
                f"stdout={stdout!r}, stderr={stderr!r}"
            )
        fields = ready.removeprefix("READY ").split()
        if len(fields) != 2:
            self.stop_server(process)
            raise AssertionError(f"invalid READY marker: {ready!r}")
        return process, int(fields[0]), fields[1]

    def finish_server(self, process: subprocess.Popen[str]) -> str:
        assert process.stdout is not None
        try:
            stdout, stderr = process.communicate(timeout=TIMEOUT_SECONDS)
        except subprocess.TimeoutExpired:
            process.terminate()
            stdout, stderr = process.communicate(timeout=TIMEOUT_SECONDS)
            raise AssertionError(
                f"server timed out; stdout={stdout!r}, stderr={stderr!r}"
            )
        if self.verbose or process.returncode != 0:
            if stdout:
                print(stdout, end="")
            if stderr:
                print(stderr, end="")
        if process.returncode != 0:
            raise AssertionError(
                f"server exited {process.returncode}: {stderr.strip()}"
            )
        return stdout

    def stop_server(self, process: subprocess.Popen[str]) -> None:
        if process.poll() is None:
            process.terminate()
        try:
            stdout, stderr = process.communicate(timeout=TIMEOUT_SECONDS)
        except subprocess.TimeoutExpired:
            process.kill()
            stdout, stderr = process.communicate(timeout=TIMEOUT_SECONDS)
        if self.verbose or process.returncode not in (0, -15):
            if stdout:
                print(stdout, end="")
            if stderr:
                print(stderr, end="")


class FakeStunServer:
    def __init__(self, malformed: bool = False) -> None:
        self.malformed = malformed
        self.socket = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        self.socket.bind(("127.0.0.1", 0))
        self.socket.settimeout(TIMEOUT_SECONDS)
        self.port = int(self.socket.getsockname()[1])
        self.error: BaseException | None = None
        self.thread = threading.Thread(target=self._serve, daemon=True)

    def __enter__(self) -> FakeStunServer:
        self.thread.start()
        return self

    def __exit__(self, *unused: object) -> None:
        self.thread.join(TIMEOUT_SECONDS)
        self.socket.close()
        if self.thread.is_alive():
            raise AssertionError("fake STUN server did not receive a request")
        if self.error is not None:
            raise self.error

    def _serve(self) -> None:
        try:
            request, peer = self.socket.recvfrom(1024)
            if len(request) != 20:
                raise AssertionError(f"unexpected STUN request length {len(request)}")
            message_type, message_length, cookie = struct.unpack("!HHI", request[:8])
            if (message_type, message_length, cookie) != (0x0001, 0, 0x2112A442):
                raise AssertionError("malformed STUN binding request")

            transaction = request[8:20]
            if self.malformed:
                transaction = bytes([transaction[0] ^ 0xFF]) + transaction[1:]
            mapped_ip = ipaddress.IPv4Address("198.51.100.42").packed
            cookie_bytes = struct.pack("!I", cookie)
            xor_ip = bytes(a ^ b for a, b in zip(mapped_ip, cookie_bytes))
            mapped_port = 45000
            value = struct.pack(
                "!BBH4s", 0, 1, mapped_port ^ (cookie >> 16), xor_ip
            )
            attribute = struct.pack("!HH", 0x0020, len(value)) + value
            response = (
                struct.pack("!HHI", 0x0101, len(attribute), cookie)
                + transaction
                + attribute
            )
            self.socket.sendto(response, peer)
        except BaseException as error:  # Propagate thread failures to the test.
            self.error = error


class LossyUdpProxy:
    """Loopback QUIC relay with deterministic server-to-client packet loss."""

    def __init__(self, target: tuple[str, int]) -> None:
        self.target = target
        self.socket = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        self.socket.bind(("127.0.0.1", 0))
        self.socket.settimeout(0.05)
        self.port = int(self.socket.getsockname()[1])
        self.client: tuple[str, int] | None = None
        self.server_packets = 0
        self.dropped = 0
        self.error: BaseException | None = None
        self.stopping = threading.Event()
        self.thread = threading.Thread(target=self._relay, daemon=True)

    def __enter__(self) -> LossyUdpProxy:
        self.thread.start()
        return self

    def __exit__(self, *unused: object) -> None:
        self.stopping.set()
        self.thread.join(TIMEOUT_SECONDS)
        self.socket.close()
        if self.thread.is_alive():
            raise AssertionError("lossy UDP proxy did not stop")
        if self.error is not None:
            raise self.error

    def _relay(self) -> None:
        try:
            while not self.stopping.is_set():
                try:
                    packet, peer = self.socket.recvfrom(65535)
                except socket.timeout:
                    continue
                if peer == self.target:
                    self.server_packets += 1
                    # Preserve handshake setup, then lose two server datagrams
                    # during the sustained asset transfer. Limiting the loss
                    # keeps this deterministic under sanitizer slowdown while
                    # still exercising QUIC recovery.
                    if self.server_packets in (20, 40):
                        self.dropped += 1
                        continue
                    if self.client is not None:
                        self.socket.sendto(packet, self.client)
                else:
                    self.client = peer
                    self.socket.sendto(packet, self.target)
        except OSError as error:
            if not self.stopping.is_set():
                self.error = error
        except BaseException as error:  # Propagate thread failures to the test.
            self.error = error


class NetworkE2ETest(unittest.TestCase):
    __unittest_skip__ = True
    __unittest_skip_why__ = "run through tests/e2e/run.py with a native driver"
    driver: NativeDriver

    def setUp(self) -> None:
        self.temporary_directory = tempfile.TemporaryDirectory(
            prefix="atrinik-network-e2e-"
        )
        self.temp = Path(self.temporary_directory.name)

    def tearDown(self) -> None:
        self.temporary_directory.cleanup()

    def test_identity(self) -> None:
        identity = self.temp / "quic-identity.pem"
        first_output = self.driver.command(
            "fingerprint", 0, identity
        ).stdout
        second_output = self.driver.command(
            "fingerprint", 0, identity
        ).stdout
        first = self.driver.marker(first_output, "FINGERPRINT ")
        second = self.driver.marker(second_output, "FINGERPRINT ")
        self.assertRegex(first, r"^[0-9a-f]{64}$")
        self.assertEqual(first, second)
        self.assertEqual(stat.S_IMODE(identity.stat().st_mode), 0o600)

        identity.write_text("not a private key\n", encoding="utf-8")
        corrupted = self.driver.command(
            "fingerprint", 0, identity, check=False
        )
        self.assertNotEqual(corrupted.returncode, 0)

    def test_quic(self) -> None:
        identity = self.temp / "quic-identity.pem"
        server, port, fingerprint = self.driver.server(identity)
        try:
            client = self.driver.command(
                "client", "127.0.0.1", port, fingerprint, PAYLOAD
            )
            server_output = self.driver.finish_server(server)
        except BaseException:
            self.driver.stop_server(server)
            raise
        client_id = self.driver.marker(client.stdout, "CLIENT ")
        server_id = self.driver.marker(server_output, "ECHO ")
        self.assertRegex(client_id, r"^[0-9a-f]{32}$")
        self.assertEqual(client_id, server_id)

        bad_server, bad_port, bad_fingerprint = self.driver.server(
            self.temp / "bad-pin-identity.pem"
        )
        replacement = "0" if bad_fingerprint[0] != "0" else "1"
        wrong_fingerprint = replacement + bad_fingerprint[1:]
        rejected = self.driver.command(
            "client",
            "127.0.0.1",
            bad_port,
            wrong_fingerprint,
            PAYLOAD,
            check=False,
        )
        self.assertNotEqual(rejected.returncode, 0)
        self.driver.stop_server(bad_server)

    def test_streams(self) -> None:
        server, port, fingerprint = self.driver.server(
            self.temp / "multi-stream-identity.pem", "streams-server"
        )
        try:
            with LossyUdpProxy(("127.0.0.1", port)) as proxy:
                client = self.driver.command(
                    "streams-client", "127.0.0.1", proxy.port, fingerprint
                )
                server_output = self.driver.finish_server(server)
            self.assertGreaterEqual(proxy.dropped, 2)
        except BaseException:
            self.driver.stop_server(server)
            raise
        marker = self.driver.marker(client.stdout, "STREAMS client ")
        self.assertRegex(marker, r"latency_ms=[0-9]+ bytes=131072 cancellation")
        self.driver.marker(server_output, "STREAMS server fairness cancellation")

    def test_stun(self) -> None:
        with FakeStunServer() as stun:
            result = self.driver.command(
                "stun",
                0,
                self.temp / "stun-identity.pem",
                f"127.0.0.1:{stun.port}",
            )
        self.assertEqual(
            self.driver.marker(result.stdout, "STUN "),
            "198.51.100.42:45000",
        )

        with FakeStunServer(malformed=True) as malformed:
            rejected = self.driver.command(
                "stun",
                0,
                self.temp / "malformed-stun-identity.pem",
                f"127.0.0.1:{malformed.port}",
                check=False,
            )
        self.assertNotEqual(rejected.returncode, 0)

    def test_disconnect(self) -> None:
        server, port, fingerprint = self.driver.server(
            self.temp / "server-close.pem", "close-server"
        )
        try:
            client = self.driver.command(
                "wait-client", "127.0.0.1", port, fingerprint
            )
            server_output = self.driver.finish_server(server)
        except BaseException:
            self.driver.stop_server(server)
            raise
        self.driver.marker(server_output, "LOCAL_CLOSE")
        client_latency = int(self.driver.marker(client.stdout, "PEER_CLOSE "))
        self.assertLess(client_latency, 1000)

        server, port, fingerprint = self.driver.server(
            self.temp / "client-close.pem", "wait-server"
        )
        try:
            client = self.driver.command(
                "close-client", "127.0.0.1", port, fingerprint
            )
            server_output = self.driver.finish_server(server)
        except BaseException:
            self.driver.stop_server(server)
            raise
        self.driver.marker(client.stdout, "LOCAL_CLOSE")
        server_latency = int(self.driver.marker(server_output, "PEER_CLOSE "))
        self.assertLess(server_latency, 1000)

    def test_punch(self) -> None:
        result = self.driver.command(
            "punch",
            0,
            self.temp / "punch-first.pem",
            0,
            self.temp / "punch-second.pem",
        )
        endpoints = self.driver.marker(result.stdout, "PUNCH ").split()
        self.assertEqual(len(endpoints), 2)
        self.assertNotEqual(endpoints[0], endpoints[1])
        for endpoint in endpoints:
            self.assertRegex(endpoint, r"^127\.0\.0\.1:[1-9][0-9]*$")

    def test_mapping(self) -> None:
        result = self.driver.command("mapping")
        self.assertEqual(
            self.driver.marker(result.stdout, "MAPPING "),
            "PCP/NAT-PMP UPnP cleanup",
        )


def parse_arguments() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--driver",
        type=Path,
        default=Path("build/linux-debug/tests/e2e/atrinik-network-e2e-driver"),
        help="path to the compiled native test driver",
    )
    parser.add_argument(
        "--scenario",
        action="append",
        choices=SCENARIOS,
        help="run only this scenario; may be supplied more than once",
    )
    parser.add_argument("--verbose", action="store_true")
    return parser.parse_args()


def main() -> int:
    arguments = parse_arguments()
    driver_path = arguments.driver.resolve()
    if not driver_path.is_file() or not os.access(driver_path, os.X_OK):
        raise SystemExit(
            f"native driver is missing or not executable: {driver_path}; "
            "build the atrinik-network-e2e-driver target first"
        )
    NetworkE2ETest.driver = NativeDriver(driver_path, arguments.verbose)
    NetworkE2ETest.__unittest_skip__ = False

    selected = arguments.scenario or list(SCENARIOS)
    suite = unittest.TestSuite(
        NetworkE2ETest(f"test_{scenario}") for scenario in selected
    )
    result = unittest.TextTestRunner(verbosity=2).run(suite)
    return 0 if result.wasSuccessful() else 1


if __name__ == "__main__":
    raise SystemExit(main())
