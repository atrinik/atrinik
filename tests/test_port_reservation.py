from __future__ import annotations

import os
from pathlib import Path
import socket
from types import SimpleNamespace
import tempfile
import threading
import time
import unittest

from atrinik_workspace.model import WorkspaceError, atomic_json
from atrinik_workspace.locking import exclusive_lock
from atrinik_workspace.port_reservation import (
    PORT_RESERVATION_DIRECTORY,
    PortReservationError,
    bind_record,
    open_lease,
    reservation_locked,
    try_lock,
    validate_held,
)
from atrinik_workspace.supervisor import _require_server_port_available
from atrinik_workspace.workspace import TOPOLOGY_PORT_RESERVATION_RECORD, Workspace


class PortReservationTests(unittest.TestCase):
    def setUp(self) -> None:
        self.temporary = tempfile.TemporaryDirectory()
        self.addCleanup(self.temporary.cleanup)
        self.topologies = Path(self.temporary.name) / "topologies"
        self.topologies.mkdir()
        self.workspace = object.__new__(Workspace)
        self.workspace.paths = SimpleNamespace(topologies=self.topologies)

    @staticmethod
    def free_port() -> int:
        with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as candidate:
            candidate.bind(("0.0.0.0", 0))
            return int(candidate.getsockname()[1])

    def test_distinct_explicit_ports_do_not_share_a_reservation_lock(self) -> None:
        ports = [self.free_port(), self.free_port()]
        while ports[0] == ports[1]:
            ports[1] = self.free_port()
        entered = threading.Barrier(3)
        release = threading.Event()
        results: list[tuple[int, dict[str, object]]] = []
        errors: list[BaseException] = []

        def reserve(index: int) -> None:
            try:
                reservation = self.workspace._reserve_topology_port(
                    ports[index], f"explicit-{index}", f"{index + 1:064x}"
                )
                results.append(reservation)
                entered.wait(timeout=2)
                release.wait(timeout=2)
            except BaseException as error:
                errors.append(error)

        threads = [threading.Thread(target=reserve, args=(index,)) for index in range(2)]
        for thread in threads:
            thread.start()
        entered.wait(timeout=2)
        self.assertEqual(errors, [])
        self.assertEqual({record["port"] for _fd, record in results}, set(ports))
        release.set()
        for thread in threads:
            thread.join(timeout=2)
        for descriptor, _record in results:
            os.close(descriptor)

    def test_concurrent_automatic_allocations_are_unique(self) -> None:
        start = threading.Barrier(3)
        held = threading.Barrier(3)
        release = threading.Event()
        results: list[tuple[int, dict[str, object]]] = []
        errors: list[BaseException] = []

        def reserve(index: int) -> None:
            try:
                start.wait(timeout=2)
                reservation = self.workspace._reserve_topology_port(
                    None, f"automatic-{index}", f"{index + 1:064x}"
                )
                results.append(reservation)
                held.wait(timeout=2)
                release.wait(timeout=2)
            except BaseException as error:
                errors.append(error)

        threads = [threading.Thread(target=reserve, args=(index,)) for index in range(2)]
        for thread in threads:
            thread.start()
        start.wait(timeout=2)
        held.wait(timeout=2)
        self.assertEqual(errors, [])
        self.assertEqual(len({record["port"] for _fd, record in results}), 2)
        release.set()
        for thread in threads:
            thread.join(timeout=2)
        for descriptor, _record in results:
            os.close(descriptor)

    def test_allocator_is_released_while_automatic_reservation_is_held(self) -> None:
        descriptor, record = self.workspace._reserve_topology_port(
            0, "automatic-zero", "5" * 64
        )
        try:
            self.assertGreater(record["port"], 0)
            with exclusive_lock(
                self.topologies / "ports.lock",
                "topology automatic port allocation",
                nonblocking=True,
            ):
                pass
        finally:
            os.close(descriptor)

    def test_explicit_port_does_not_enter_busy_automatic_allocator(self) -> None:
        port = self.free_port()
        with exclusive_lock(
            self.topologies / "ports.lock",
            "topology automatic port allocation",
            nonblocking=True,
        ):
            descriptor, record = self.workspace._reserve_topology_port(
                port, "explicit-independent", "6" * 64
            )
        try:
            self.assertEqual(record["port"], port)
        finally:
            os.close(descriptor)

    def test_same_explicit_port_has_one_winner_and_names_it(self) -> None:
        port = self.free_port()
        start = threading.Barrier(3)
        release = threading.Event()
        results: list[tuple[int, dict[str, object]]] = []
        errors: list[str] = []

        def reserve(index: int) -> None:
            try:
                start.wait(timeout=2)
                reservation = self.workspace._reserve_topology_port(
                    port, f"same-{index}", f"{index + 1:064x}"
                )
                results.append(reservation)
                release.wait(timeout=2)
            except WorkspaceError as error:
                errors.append(str(error))

        threads = [threading.Thread(target=reserve, args=(index,)) for index in range(2)]
        for thread in threads:
            thread.start()
        start.wait(timeout=2)
        deadline = time.monotonic() + 2
        while time.monotonic() < deadline and len(results) + len(errors) < 2:
            time.sleep(0.01)
        self.assertEqual(len(results), 1)
        self.assertEqual(len(errors), 1)
        winner = results[0][1]
        self.assertIn(f"topology {winner['topology']}", errors[0])
        self.assertIn(str(winner["generation"]), errors[0])
        release.set()
        for thread in threads:
            thread.join(timeout=2)
        os.close(results[0][0])

    def test_released_and_aborted_reservations_can_be_reacquired(self) -> None:
        port = self.free_port()
        descriptor, _record = self.workspace._reserve_topology_port(
            port, "aborted", "1" * 64
        )
        os.close(descriptor)
        replacement, record = self.workspace._reserve_topology_port(
            port, "replacement", "2" * 64
        )
        try:
            self.assertEqual(record["topology"], "replacement")
        finally:
            os.close(replacement)

    def test_external_claim_after_reservation_is_actionable(self) -> None:
        port = self.free_port()
        descriptor, record = self.workspace._reserve_topology_port(
            port, "external-race", "3" * 64
        )
        try:
            with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as external:
                external.bind(("0.0.0.0", port))
                with self.assertRaisesRegex(
                    RuntimeError, "external process before server startup"
                ):
                    _require_server_port_available(
                        {"endpoint": {"host": "127.0.0.1", "port": record["port"]}}
                    )
        finally:
            os.close(descriptor)

    def test_lease_rejects_links_and_record_replacement(self) -> None:
        directory = self.topologies / PORT_RESERVATION_DIRECTORY
        directory.mkdir(mode=0o700)
        target = directory / "target"
        target.write_text("valuable\n", encoding="utf-8")
        target.chmod(0o600)
        symlink = directory / "17300.lease"
        symlink.symlink_to(target)
        with self.assertRaisesRegex(PortReservationError, "cannot open"):
            open_lease(self.topologies, 17300)
        symlink.unlink()
        hardlink = directory / "17301.lease"
        os.link(target, hardlink)
        with self.assertRaisesRegex(PortReservationError, "identity is invalid"):
            open_lease(self.topologies, 17301)

        port = self.free_port()
        descriptor, path = open_lease(self.topologies, port)
        self.assertTrue(try_lock(descriptor))
        record = bind_record(
            descriptor,
            path,
            port=port,
            topology="replacement-test",
            generation="4" * 64,
        )
        path.unlink()
        path.write_text("{}\n", encoding="utf-8")
        path.chmod(0o600)
        try:
            with self.assertRaisesRegex(PortReservationError, "identity is invalid"):
                validate_held(descriptor, record)
        finally:
            os.close(descriptor)

    def test_pending_evidence_prevents_replaced_lease_from_being_stolen(self) -> None:
        port = self.free_port()
        descriptor, record = self.workspace._reserve_topology_port(
            port, "original", "7" * 64
        )
        owner = self.topologies / "original"
        owner.mkdir()
        atomic_json(owner / TOPOLOGY_PORT_RESERVATION_RECORD, record)
        path = Path(record["path"])
        path.unlink()
        path.write_text("{}\n", encoding="utf-8")
        path.chmod(0o600)
        try:
            with self.assertRaisesRegex(
                WorkspaceError, "does not match its exact lease"
            ):
                self.workspace._reserve_topology_port(
                    port, "attacker", "8" * 64
                )
        finally:
            os.close(descriptor)

    def test_unlocked_pending_evidence_is_reusable_without_pid_observation(self) -> None:
        port = self.free_port()
        descriptor, record = self.workspace._reserve_topology_port(
            port, "stopped", "9" * 64
        )
        owner = self.topologies / "stopped"
        owner.mkdir()
        atomic_json(owner / TOPOLOGY_PORT_RESERVATION_RECORD, record)
        os.close(descriptor)

        replacement, replacement_record = self.workspace._reserve_topology_port(
            port, "restarted", "a" * 64
        )
        try:
            self.assertEqual(replacement_record["topology"], "restarted")
        finally:
            os.close(replacement)

    def test_reused_lease_reports_its_current_owner_not_stale_evidence(self) -> None:
        port = self.free_port()
        descriptor, record = self.workspace._reserve_topology_port(
            port, "stale-owner", "b" * 64
        )
        owner = self.topologies / "stale-owner"
        owner.mkdir()
        atomic_json(owner / TOPOLOGY_PORT_RESERVATION_RECORD, record)
        os.close(descriptor)

        replacement, replacement_record = self.workspace._reserve_topology_port(
            port, "current-owner", "c" * 64
        )
        try:
            with self.assertRaises(WorkspaceError) as raised:
                self.workspace._reserve_topology_port(
                    port, "contender", "d" * 64
                )
            message = str(raised.exception)
            self.assertIn("topology current-owner", message)
            self.assertIn(str(replacement_record["generation"]), message)
            self.assertNotIn("topology stale-owner", message)
        finally:
            os.close(replacement)

    def test_reused_locked_inode_is_retained_only_for_current_record(self) -> None:
        port = self.free_port()
        descriptor, old_record = self.workspace._reserve_topology_port(
            port, "old-status", "e" * 64
        )
        os.close(descriptor)
        replacement, current_record = self.workspace._reserve_topology_port(
            port, "new-status", "f" * 64
        )
        try:
            self.assertFalse(reservation_locked(old_record))
            self.assertTrue(reservation_locked(current_record))
        finally:
            os.close(replacement)


if __name__ == "__main__":
    unittest.main()
