import unittest

import Atrinik
from tests import TestSuite


class RegionFieldsSuite(TestSuite):
    def setUp(self):
        super().setUp()
        self.region = self.obj = Atrinik.GetFirst("region")

    def test_next(self):
        with self.assertRaises(AttributeError):
            self.region.next = None

        self.assertIsNotNone(self.region.next)

        r = self.region
        while r.next:
            r = r.next

        self.assertIsNone(r.next)

    def test_parent(self):
        with self.assertRaises(AttributeError):
            self.region.parent = None

        self.assertIsNone(self.region.parent)

    def test_name(self):
        with self.assertRaises(AttributeError):
            self.region.name = None

        self.assertEqual(self.region.name, "world")
        m = Atrinik.ReadyMap("/hall_of_dms")
        self.assertEqual(m.region.name, "creation")

    def test_longname(self):
        with self.assertRaises(AttributeError):
            self.region.longname = None

        self.assertEqual(self.region.longname, "The World")
        m = Atrinik.ReadyMap("/hall_of_dms")
        self.assertEqual(m.region.longname, "Plane of Creation")

    def test_msg(self):
        with self.assertRaises(AttributeError):
            self.region.msg = None

        self.assertEqual(self.region.msg, "Somewhere in the world...\n")

    def test_jailmap(self):
        with self.assertRaises(AttributeError):
            self.region.jailmap = None

        self.assertIn("jail", self.region.jailmap)

    def test_jailx(self):
        with self.assertRaises(AttributeError):
            self.region.jailx = None

        self.assertNotEqual(self.region.jailx, 0)

    def test_jaily(self):
        with self.assertRaises(AttributeError):
            self.region.jaily = None

        self.assertNotEqual(self.region.jaily, 0)

    def test_map_first(self):
        with self.assertRaises(AttributeError):
            self.region.map_first = None

        self.assertIsNone(self.region.map_first)
        m = Atrinik.ReadyMap("/shattered_islands/world_1_67")
        self.assertEqual(m.region.name, "brynknot")
        r = m.region.parent
        self.assertEqual(r.name, "strakewood_island")
        self.assertEqual(r.map_first, "/shattered_islands/world_4_40")


activator = Atrinik.WhoIsActivator()
me = Atrinik.WhoAmI()
suites = [
    unittest.TestLoader().loadTestsFromTestCase(RegionFieldsSuite),
]
