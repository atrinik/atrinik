import unittest

import Atrinik


class ObjectTestCase(unittest.TestCase):
    def setUp(self):
        self.obj = Atrinik.CreateObject("sword")

    def tearDown(self):
        self.obj.Destroy()

    def test_ActivateRune(self):
        self.assertRaises(TypeError, self.obj.ActivateRune)
        self.assertRaises(TypeError, self.obj.ActivateRune, 1, 2)
        self.assertRaises(TypeError, self.obj.ActivateRune, x=1)

        self.assertRaises(TypeError, self.obj.ActivateRune, activator)

        rune = Atrinik.CreateObject("rune_fire")
        hp = activator.hp
        rune.ActivateRune(activator)
        rune.Destroy()
        self.assertLessEqual(activator.hp, hp)

        m = Atrinik.CreateMap(5, 5, "test-atrinik-object-activate-rune")
        rune = m.CreateObject("rune_fire", 0, 0)
        hp = activator.hp
        rune.ActivateRune(activator)
        self.assertLessEqual(activator.hp, hp)
        self.assertEqual(rune.layer, Atrinik.LAYER_EFFECT)
        self.assertTrue(rune.f_is_used_up)

    def test_TeleportTo(self):
        self.assertRaises(TypeError, self.obj.TeleportTo)
        self.assertRaises(TypeError, self.obj.TeleportTo, 1, 2)
        self.assertRaises(TypeError, self.obj.TeleportTo, x=1)

        self.obj.TeleportTo("/emergency")
        self.assertNotEqual(self.obj.map.path, "/emergency")

        self.obj.f_no_teleport = True
        self.obj.TeleportTo("/emergency")
        self.assertEqual(self.obj.map.path, "/emergency")

        m = Atrinik.CreateMap(5, 5, "test-atrinik-object-teleport-to")
        self.obj.TeleportTo(m.path)
        self.assertEqual(self.obj.map.path, m.path)
        self.assertEqual(self.obj.x, 0)
        self.assertEqual(self.obj.y, 0)

        self.obj.TeleportTo(m.path, 2, 3)
        self.assertEqual(self.obj.map.path, m.path)
        self.assertEqual(self.obj.x, 2)
        self.assertEqual(self.obj.y, 3)



activator = Atrinik.WhoIsActivator()
me = Atrinik.WhoAmI()
suites = [
    unittest.TestLoader().loadTestsFromTestCase(ObjectTestCase)
]
