import unittest

import Atrinik
from tests import TestSuite


class ArchetypeFieldsSuite(TestSuite):
    def setUp(self):
        super().setUp()
        self.arch = self.obj = Atrinik.GetArchetype("sword")

    def test_name(self):
        with self.assertRaises(AttributeError):
            self.arch.name = None

        self.assertEqual(self.arch.name, "sword")

    def test_head(self):
        with self.assertRaises(AttributeError):
            self.arch.head = None

        self.assertIsNone(self.arch.head)
        arch = Atrinik.GetArchetype("gazer_dread")
        self.assertIsNone(arch.head)
        self.assertEqual(arch.more.head, arch)

    def test_more(self):
        with self.assertRaises(AttributeError):
            self.arch.more = None

        self.assertIsNone(self.arch.more)
        arch = Atrinik.GetArchetype("gazer_dread")
        self.assertIsNotNone(arch.more)
        self.assertNotEqual(arch.more, arch)

    def test_clone(self):
        with self.assertRaises(AttributeError):
            self.arch.clone = None

        self.assertIsNotNone(self.arch.clone)
        self.assertEqual(self.arch.clone.name, "sword")


activator = Atrinik.WhoIsActivator()
me = Atrinik.WhoAmI()
suites = [
    unittest.TestLoader().loadTestsFromTestCase(ArchetypeFieldsSuite),
]
