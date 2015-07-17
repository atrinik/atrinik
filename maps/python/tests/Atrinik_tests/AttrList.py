import unittest

import Atrinik
from tests import simulate_server


class AttrListSuite(unittest.TestCase):
    maxDiff = None

    def setUp(self):
        simulate_server(count=1, wait=False)
        self.pl = me.Controller()
        self.tearDown()

    def tearDown(self):
        self.pl.cmd_permissions.clear()
        self.pl.factions.clear()
        self.pl.s_packets.clear()

    def test_cmd_permissions(self):
        self.assertEqual(self.pl.cmd_permissions.items(), [])
        self.pl.cmd_permissions.append("[OP]")
        self.assertEqual(self.pl.cmd_permissions.items(), ["[OP]"])
        self.assertEqual(len(self.pl.cmd_permissions), 1)
        self.pl.cmd_permissions[1] = "[OP]"
        self.assertEqual(len(self.pl.cmd_permissions), 2)
        self.assertEqual(self.pl.cmd_permissions.items(), ["[OP]", "[OP]"])
        for perm in self.pl.cmd_permissions:
            self.assertEqual(perm, "[OP]")
        self.pl.cmd_permissions[0] = "[DEV]"
        perms = ["[DEV]", "[OP]"]
        self.assertEqual(self.pl.cmd_permissions.items(), perms)
        self.assertEqual(len(self.pl.cmd_permissions), 2)
        for i, perm in enumerate(self.pl.cmd_permissions):
            self.assertEqual(perm, perms[i])
        self.pl.cmd_permissions.append("[MOD]")
        perms.append("[MOD]")
        self.assertEqual(self.pl.cmd_permissions.items(), perms)
        self.assertEqual(len(self.pl.cmd_permissions), 3)
        for i, perm in enumerate(self.pl.cmd_permissions):
            self.assertEqual(perm, perms[i])
        self.pl.cmd_permissions.remove("[MOD]")
        perms = [perm if perm != "[MOD]" else None for perm in perms]
        self.assertEqual(self.pl.cmd_permissions.items(), perms)
        self.assertEqual(len(self.pl.cmd_permissions), 3)
        for i, perm in enumerate(self.pl.cmd_permissions):
            self.assertEqual(perm, perms[i])
        self.pl.cmd_permissions.clear()
        self.assertEqual(self.pl.cmd_permissions.items(), [])
        self.assertEqual(len(self.pl.cmd_permissions), 0)
        for perm in self.pl.cmd_permissions:
            self.fail("Found invalid permission: {}".format(perm))

    def test_factions(self):
        self.assertEqual(self.pl.factions.items(), [])
        self.pl.factions["asteria"] = 500
        self.assertEqual(self.pl.factions.items(), [("asteria", 500.0)])
        self.pl.factions["asteria"] += 50
        self.assertEqual(self.pl.factions.items(), [("asteria", 550.0)])
        self.pl.factions["brynknot"] = 120
        self.assertEqual(self.pl.factions.items(), [("asteria", 550.0),
                                                    ("brynknot", 120.0)])
        del self.pl.factions["brynknot"]
        self.assertEqual(self.pl.factions.items(), [("asteria", 550.0)])
        with self.assertRaises(KeyError):
            del self.pl.factions["brynknot"]
        with self.assertRaises(NotImplementedError):
            self.pl.factions.append("test")
        self.pl.factions["brynknot"] = -450.23
        factions = [("asteria", 550.0), ("brynknot", -450.23)]
        self.assertEqual(self.pl.factions.items(), factions)
        for i, faction in enumerate(self.pl.factions):
            self.assertEqual(faction, factions[i][1])
        self.pl.factions.clear()
        for faction in self.pl.factions:
            self.fail("Found invalid faction: {}".format(faction))
        self.assertEqual(self.pl.factions.items(), [])

    def test_packets(self):
        with self.assertRaises(NotImplementedError):
            self.pl.s_packets.items()
        self.assertEqual(len(self.pl.s_packets), 0)
        packet = "hello world".encode()
        self.pl.s_packets.append(packet)
        self.assertEqual(self.pl.s_packets[0], packet)
        self.assertEqual(len(self.pl.s_packets), 1)
        activator.Say("hello world!")
        self.assertIn("hello world!".encode(), self.pl.s_packets[2])
        self.assertEqual(len(self.pl.s_packets), 3)
        self.pl.s_packets.clear()
        self.assertEqual(len(self.pl.s_packets), 0)


class AttrListMethodsSuite(unittest.TestCase):
    maxDiff = None

    def setUp(self):
        simulate_server(count=1, wait=False)
        self.pl = me.Controller()
        self.tearDown()

    def tearDown(self):
        self.pl.cmd_permissions.clear()

    def test_append(self):
        with self.assertRaises(TypeError):
            self.pl.cmd_permissions.append(5000)

        self.pl.cmd_permissions.append("[OP]")
        for node in self.pl.cmd_permissions:
            self.assertEqual(node, "[OP]")
        self.assertEqual(self.pl.cmd_permissions.items(), ["[OP]"])
        self.pl.cmd_permissions.remove("[OP]")
        self.assertEqual(self.pl.cmd_permissions.items(), [None])

    def test_remove(self):
        self.pl.cmd_permissions.append("[OP]")
        self.assertEqual(self.pl.cmd_permissions.items(), ["[OP]"])
        self.pl.cmd_permissions.remove("[OP]")
        self.assertEqual(self.pl.cmd_permissions.items(), [None])
        self.pl.cmd_permissions.append("[OP]")
        self.pl.cmd_permissions.append("[OP]")
        self.assertEqual(self.pl.cmd_permissions.items(),
            [None, "[OP]", "[OP]"])
        self.pl.cmd_permissions.remove("[OP]")
        self.assertEqual(self.pl.cmd_permissions.items(),
            [None, None, "[OP]"])
        self.pl.cmd_permissions.remove("[OP]")
        self.assertEqual(self.pl.cmd_permissions.items(),
            [None, None, None])
        self.pl.cmd_permissions.append("[OP]")
        self.pl.cmd_permissions.append("[DEV]")
        self.pl.cmd_permissions.append("[MOD]")
        self.assertEqual(self.pl.cmd_permissions.items(),
            [None, None, None, "[OP]", "[DEV]", "[MOD]"])
        self.pl.cmd_permissions.remove("[DEV]")
        self.assertEqual(self.pl.cmd_permissions.items(),
            [None, None, None, "[OP]", None, "[MOD]"])
        self.pl.cmd_permissions.remove("[MOD]")
        self.assertEqual(self.pl.cmd_permissions.items(),
            [None, None, None, "[OP]", None, None])
        self.pl.cmd_permissions.remove("[OP]")
        self.assertEqual(self.pl.cmd_permissions.items(),
            [None, None, None, None, None, None])

    def test_clear(self):
        self.assertEqual(self.pl.cmd_permissions.items(), [])
        self.pl.cmd_permissions.append("[OP]")
        self.assertEqual(self.pl.cmd_permissions.items(), ["[OP]"])
        self.pl.cmd_permissions.clear()
        self.assertEqual(self.pl.cmd_permissions.items(), [])
        self.pl.cmd_permissions.append("[OP]")
        self.pl.cmd_permissions.append("[OP]")
        self.pl.cmd_permissions.append("[OP]")
        self.assertEqual(self.pl.cmd_permissions.items(),
            ["[OP]", "[OP]", "[OP]"])
        self.pl.cmd_permissions.clear()
        self.assertEqual(self.pl.cmd_permissions.items(), [])
        self.pl.cmd_permissions.append("[OP]")
        self.pl.cmd_permissions.append("[DEV]")
        self.pl.cmd_permissions.append("[MOD]")
        self.assertEqual(self.pl.cmd_permissions.items(),
            ["[OP]", "[DEV]", "[MOD]"])
        self.pl.cmd_permissions.clear()
        self.assertEqual(self.pl.cmd_permissions.items(), [])

    def test_items(self):
        self.assertEqual(self.pl.cmd_permissions.items(), [])
        self.pl.cmd_permissions.append("[OP]")
        self.pl.cmd_permissions.append("[DEV]")
        self.pl.cmd_permissions.append("[MOD]")
        self.assertEqual(self.pl.cmd_permissions.items(),
            ["[OP]", "[DEV]", "[MOD]"])

    def test_getitem(self):
        with self.assertRaises(IndexError):
            # noinspection PyStatementEffect
            self.pl.cmd_permissions[0]

        self.pl.cmd_permissions.append("[OP]")
        self.assertEqual(self.pl.cmd_permissions[0], "[OP]")
        self.pl.cmd_permissions.append("[DEV]")
        self.assertEqual(self.pl.cmd_permissions[0], "[OP]")
        self.assertEqual(self.pl.cmd_permissions[1], "[DEV]")

    def test_setitem(self):
        with self.assertRaises(IndexError):
            self.pl.cmd_permissions[1] = "DEV"
        with self.assertRaises(TypeError):
            self.pl.cmd_permissions[0] = 50

        self.pl.cmd_permissions[0] = "[OP]"
        self.assertEqual(self.pl.cmd_permissions[0], "[OP]")
        self.pl.cmd_permissions[0] = "[DEV]"
        self.assertEqual(self.pl.cmd_permissions[0], "[DEV]")
        self.pl.cmd_permissions[1] = "[MOD]"
        self.assertEqual(self.pl.cmd_permissions[0], "[DEV]")
        self.assertEqual(self.pl.cmd_permissions[1], "[MOD]")

    def test_delitem(self):
        with self.assertRaises(NotImplementedError):
            del self.pl.cmd_permissions[0]
        with self.assertRaises(TypeError):
            del self.pl.factions[0]
        with self.assertRaises(KeyError):
            del self.pl.factions["brynknot"]

        self.pl.factions["brynknot"] = 10
        self.assertEqual(self.pl.factions.items(), [("brynknot", 10.0)])
        del self.pl.factions["brynknot"]
        self.assertEqual(self.pl.factions.items(), [])


    def test_contains(self):
        self.assertNotIn("[OP]", self.pl.cmd_permissions)
        self.pl.cmd_permissions.append("[OP]")
        self.assertIn("[OP]", self.pl.cmd_permissions)
        self.pl.cmd_permissions.append("[DEV]")
        self.assertIn("[OP]", self.pl.cmd_permissions)
        self.assertIn("[DEV]", self.pl.cmd_permissions)

    def test_iter(self):
        for perm in self.pl.cmd_permissions:
            self.fail("Found invalid permission: {}".format(perm))

        perms = list()
        perms.append("[OP]")
        self.pl.cmd_permissions.append("[OP]")
        for i, perm in enumerate(self.pl.cmd_permissions):
            self.assertEqual(perm, perms[i])
        perms.append("[DEV]")
        self.pl.cmd_permissions.append("[DEV]")
        for i, perm in enumerate(self.pl.cmd_permissions):
            self.assertEqual(perm, perms[i])
        perms.append("[MOD]")
        self.pl.cmd_permissions.append("[MOD]")
        for i, perm in enumerate(self.pl.cmd_permissions):
            self.assertEqual(perm, perms[i])

    def test_len(self):
        self.assertEqual(len(self.pl.cmd_permissions), 0)
        self.pl.cmd_permissions.append("[OP]")
        self.assertEqual(len(self.pl.cmd_permissions), 1)
        self.pl.cmd_permissions.append("[DEV]")
        self.assertEqual(len(self.pl.cmd_permissions), 2)
        self.pl.cmd_permissions.remove("[OP]")
        self.assertEqual(len(self.pl.cmd_permissions), 2)
        self.pl.cmd_permissions.clear()
        self.assertEqual(len(self.pl.cmd_permissions), 0)


activator = Atrinik.WhoIsActivator()
me = Atrinik.WhoAmI()
suites = [
    unittest.TestLoader().loadTestsFromTestCase(AttrListSuite),
    unittest.TestLoader().loadTestsFromTestCase(AttrListMethodsSuite),
]
