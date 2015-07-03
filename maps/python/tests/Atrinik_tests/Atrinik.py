import unittest

import Atrinik


class AtrinikTestCase(unittest.TestCase):
    def test_GetTicks(self):
        self.assertGreaterEqual(Atrinik.GetTicks(), 0, "Incorrect ticks value")
        self.assertRaises(TypeError, Atrinik.GetTicks, args=(1, 2))
        self.assertRaises(TypeError, Atrinik.GetTicks, kwargs={"x": 1})


suite = unittest.TestLoader().loadTestsFromTestCase(AtrinikTestCase)
