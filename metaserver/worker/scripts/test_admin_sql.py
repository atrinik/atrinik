import argparse
import json
import tempfile
import unittest
from pathlib import Path

import admin_sql


SERVER_ID = "1" * 64


class AdminSqlTest(unittest.TestCase):
    def write_records(self, records: object) -> Path:
        temporary = tempfile.NamedTemporaryFile(mode="w", suffix=".json", delete=False)
        with temporary:
            json.dump(records, temporary)
        self.addCleanup(Path(temporary.name).unlink)
        return Path(temporary.name)

    def test_import_owners_emits_escaped_reviewable_sql(self) -> None:
        path = self.write_records([{
            "server_id": SERVER_ID.upper(),
            "auth_key": "a" * 128,
            "current_ip": "2001:0db8::1",
            "created_at": 10,
            "updated_at": 20,
        }])
        sql = admin_sql.command_import_owners(argparse.Namespace(input=path))
        self.assertIn("INSERT INTO server_owners", sql)
        self.assertIn(f"'{SERVER_ID}'", sql)
        self.assertIn("'2001:db8::1'", sql)

    def test_import_rejects_duplicate_identity_and_bad_hash(self) -> None:
        duplicate = {
            "server_id": SERVER_ID,
            "auth_key": "a" * 128,
            "current_ip": "192.0.2.1",
        }
        path = self.write_records([duplicate, duplicate])
        with self.assertRaisesRegex(ValueError, "duplicate server identity"):
            admin_sql.load_owner_records(path)

        malformed = dict(duplicate, auth_key="raw")
        with self.assertRaisesRegex(ValueError, "auth_key"):
            admin_sql.owner_insert(malformed)

    def test_reset_owner_is_scoped_to_one_identity(self) -> None:
        sql = admin_sql.command_reset_owner(
            argparse.Namespace(server_id=SERVER_ID.upper()),
        )
        self.assertIn(f"DELETE FROM servers WHERE server_id = '{SERVER_ID}'", sql)
        self.assertIn(f"DELETE FROM server_owners WHERE server_id = '{SERVER_ID}'", sql)

    def test_blacklist_sql_escapes_operator_input(self) -> None:
        sql = admin_sql.command_blacklist_add(
            argparse.Namespace(pattern="1111*", reason="operator's test"),
        )
        self.assertIn("operator''s test", sql)
        self.assertIn("ON CONFLICT(pattern)", sql)


if __name__ == "__main__":
    unittest.main()
