from __future__ import annotations

import fcntl
import json
import os
from pathlib import Path
import socket
from types import SimpleNamespace
import tempfile
import threading
import time
import unittest
from unittest import mock

from atrinik_workspace.model import WorkspaceError, atomic_json
from atrinik_workspace.locking import exclusive_lock
from atrinik_workspace.port_reservation import (
    PORT_RESERVATION_DIRECTORY,
    PortReservationError,
    active_owner,
    create_lease,
    open_directory,
    open_transaction,
    reservation_locked,
    try_lock,
    validate_held,
    validate_transaction,
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
        symlink = directory / "17300.lock"
        symlink.symlink_to(target)
        with self.assertRaisesRegex(PortReservationError, "cannot open"):
            open_transaction(self.topologies, 17300)
        symlink.unlink()
        hardlink = directory / "17301.lock"
        os.link(target, hardlink)
        with self.assertRaisesRegex(PortReservationError, "identity is invalid"):
            open_transaction(self.topologies, 17301)

        port = self.free_port()
        directory_fd, directory, identity = open_directory(self.topologies)
        descriptor, record = create_lease(
            directory_fd,
            directory,
            identity,
            port=port,
            topology="replacement-test",
            generation="4" * 64,
        )
        os.close(directory_fd)
        path = Path(record["path"])
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

    def test_immutable_generations_are_observed_independently(self) -> None:
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

    def test_concurrent_status_observers_do_not_impersonate_owner(self) -> None:
        port = self.free_port()
        descriptor, record = self.workspace._reserve_topology_port(
            port, "stopped-probe", "1" * 64
        )
        os.close(descriptor)
        path = Path(record["path"])
        probe = os.open(path, os.O_RDONLY | os.O_CLOEXEC)
        try:
            fcntl.flock(probe, fcntl.LOCK_SH | fcntl.LOCK_NB)
            self.assertFalse(reservation_locked(record))
            replacement, replacement_record = self.workspace._reserve_topology_port(
                port, "after-probe", "2" * 64
            )
            try:
                self.assertEqual(replacement_record["topology"], "after-probe")
            finally:
                os.close(replacement)
        finally:
            os.close(probe)

    def test_directory_substitution_cannot_redirect_child_open(self) -> None:
        directory_fd, directory, identity = open_directory(self.topologies)
        moved = self.topologies / "original-reservations"
        directory.rename(moved)
        attacker = self.topologies / "attacker"
        attacker.mkdir(mode=0o700)
        valuable = attacker / "valuable"
        valuable.write_text("valuable\n", encoding="utf-8")
        valuable.chmod(0o600)
        directory.symlink_to(attacker, target_is_directory=True)
        try:
            with self.assertRaisesRegex(PortReservationError, "was replaced"):
                create_lease(
                    directory_fd,
                    directory,
                    identity,
                    port=17300,
                    topology="safe-owner",
                    generation="3" * 64,
                )
            self.assertEqual(valuable.read_text(encoding="utf-8"), "valuable\n")
        finally:
            os.close(directory_fd)

    def test_prepublication_owner_is_not_visible_until_immutable_record_exists(self) -> None:
        port = 17379
        transaction, directory_fd, directory, _identity = open_transaction(
            self.topologies, port
        )
        try:
            self.assertTrue(try_lock(transaction))
            self.assertIsNone(active_owner(directory_fd, directory, port))
        finally:
            os.close(transaction)
            os.close(directory_fd)

    def test_creation_token_rejects_identity_matching_replacement(self) -> None:
        directory_fd, directory, identity = open_directory(self.topologies)
        descriptor, record = create_lease(
            directory_fd,
            directory,
            identity,
            port=17380,
            topology="token-owner",
            generation="4" * 64,
        )
        path = Path(record["path"])
        path.unlink()
        path.write_text("{}\n", encoding="utf-8")
        path.chmod(0o600)
        replacement = os.open(path, os.O_RDWR | os.O_CLOEXEC)
        forged = dict(record)
        replacement_metadata = os.fstat(replacement)
        forged["lease"] = {
            "device": replacement_metadata.st_dev,
            "inode": replacement_metadata.st_ino,
        }
        payload = (json.dumps(forged, indent=2, sort_keys=True) + "\n").encode()
        os.ftruncate(replacement, 0)
        os.write(replacement, payload)
        try:
            with self.assertRaisesRegex(PortReservationError, "creation identity"):
                validate_held(replacement, forged)
        finally:
            os.close(replacement)
            os.close(descriptor)
            os.close(directory_fd)

    def test_replaced_transaction_cannot_publish_two_owners(self) -> None:
        port = 17381
        first, first_directory, _directory, _identity = open_transaction(
            self.topologies, port
        )
        self.assertTrue(try_lock(first))
        transaction_path = (
            self.topologies / PORT_RESERVATION_DIRECTORY / f"{port}.lock"
        )
        transaction_path.unlink()
        second, second_directory, _directory, _identity = open_transaction(
            self.topologies, port
        )
        try:
            self.assertTrue(try_lock(second))
            with self.assertRaisesRegex(PortReservationError, "identity is invalid"):
                validate_transaction(first, first_directory, port)
            validate_transaction(second, second_directory, port)
        finally:
            os.close(first)
            os.close(first_directory)
            os.close(second)
            os.close(second_directory)

    def test_failed_publication_preserves_replacement_path(self) -> None:
        directory_fd, directory, identity = open_directory(self.topologies)
        valuable = directory / "valuable"
        valuable.write_text("valuable\n", encoding="utf-8")
        valuable.chmod(0o600)
        generation = "5" * 64
        lease = directory / f"17382-{generation}.lease"
        real_fsync = os.fsync

        def replace_before_validation(descriptor: int) -> None:
            real_fsync(descriptor)
            lease.unlink()
            valuable.rename(lease)

        try:
            with (
                mock.patch(
                    "atrinik_workspace.port_reservation.os.fsync",
                    side_effect=replace_before_validation,
                ),
                self.assertRaisesRegex(PortReservationError, "identity is invalid"),
            ):
                create_lease(
                    directory_fd,
                    directory,
                    identity,
                    port=17382,
                    topology="cleanup-owner",
                    generation=generation,
                )
            self.assertEqual(lease.read_text(encoding="utf-8"), "valuable\n")
        finally:
            os.close(directory_fd)


if __name__ == "__main__":
    unittest.main()
