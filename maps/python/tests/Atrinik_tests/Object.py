import random
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

    def test_InsertInto(self):
        self.assertRaises(TypeError, self.obj.InsertInto)
        self.assertRaises(TypeError, self.obj.InsertInto, 1, 2)
        self.assertRaises(TypeError, self.obj.InsertInto, x=1)

        container = Atrinik.CreateObject("sack")
        self.assertFalse(container.inv)
        self.obj.f_can_stack = False
        obj2 = self.obj.Clone()
        self.obj.InsertInto(container)
        self.assertTrue(container.inv)
        self.assertEqual(len(container.inv), 1)
        self.assertEqual(container.inv[0], self.obj)
        self.assertIn(self.obj, container.inv)
        obj2.InsertInto(container)
        self.assertTrue(container.inv)
        self.assertEqual(len(container.inv), 2)
        self.assertEqual(container.inv[0], obj2)
        self.assertEqual(container.inv[1], self.obj)
        self.assertIn(self.obj, container.inv)
        self.assertIn(obj2, container.inv)
        self.obj.Remove()
        container.Destroy()

    def test_Apply(self):
        self.assertRaises(TypeError, self.obj.Apply)
        self.assertRaises(TypeError, self.obj.Apply, 1, 2)
        self.assertRaises(TypeError, self.obj.Apply, x=1)

        self.obj.InsertInto(activator)
        self.obj.item_level = 0
        self.obj.item_skill = 0
        activator.Apply(self.obj)
        self.assertTrue(self.obj.f_applied)
        activator.Apply(self.obj)
        self.assertFalse(self.obj.f_applied)

    def test_Take(self):
        self.assertRaises(TypeError, self.obj.Take)
        self.assertRaises(TypeError, self.obj.Take, 1, 2)
        self.assertRaises(TypeError, self.obj.Take, x=1)

        m = Atrinik.CreateMap(5, 5, "test-atrinik-object-take")
        m.Insert(self.obj, 0, 0)
        m.Insert(activator, 0, 0)
        activator.Take(self.obj)
        self.assertEqual(self.obj.env, activator)

        m.Insert(self.obj, 0, 0)
        self.assertIsNone(self.obj.env)

        activator.Take("all")
        self.assertEqual(self.obj.env, activator)

    def test_Drop(self):
        self.assertRaises(TypeError, self.obj.Drop)
        self.assertRaises(TypeError, self.obj.Drop, 1, 2)
        self.assertRaises(TypeError, self.obj.Drop, x=1)

        m = Atrinik.CreateMap(5, 5, "test-atrinik-object-drop")
        m.Insert(activator, 0, 0)
        self.obj.InsertInto(activator)
        self.assertEqual(self.obj.env, activator)
        activator.Drop(self.obj)
        self.assertIsNone(self.obj.env)
        self.assertEqual(self.obj.map, m)

        self.obj.InsertInto(activator)
        self.assertEqual(self.obj.env, activator)
        activator.Drop("sword")
        self.assertIsNone(self.obj.env)
        self.assertEqual(self.obj.map, m)

    def test_Say(self):
        self.assertRaises(TypeError, self.obj.Say)
        self.assertRaises(TypeError, self.obj.Say, 1, 2)
        self.assertRaises(TypeError, self.obj.Say, x=1)

        m = Atrinik.CreateMap(5, 5, "test-atrinik-object-drop")
        m.Insert(activator, 0, 0)
        m.Insert(me, 4, 4)
        m.Insert(self.obj, 1, 1)
        activator.Controller().s_packets.clear()
        me.Controller().s_packets.clear()
        msg = "Hello world: {}!".format(random.random())
        self.obj.Say(msg)
        msg = "{} says: {}".format(self.obj.GetName(), msg)

        for obj in (activator, me):
            packets = obj.Controller().s_packets
            data = packets[len(packets) - 1][8:-1].decode('utf-8')
            self.assertEqual(data, msg)

    def test_getGender(self):
        self.assertRaises(TypeError, self.obj.GetGender, 1, 2)
        self.assertRaises(TypeError, self.obj.GetGender, x=1)

        self.assertEqual(self.obj.GetGender(), Atrinik.Gender.NEUTER)
        self.obj.f_is_male = True
        self.assertEqual(self.obj.GetGender(), Atrinik.Gender.MALE)
        self.obj.f_is_female = True
        self.assertEqual(self.obj.GetGender(), Atrinik.Gender.HERMAPHRODITE)
        self.obj.f_is_male = False
        self.assertEqual(self.obj.GetGender(), Atrinik.Gender.FEMALE)

    def test_setGender(self):
        self.assertRaises(TypeError, self.obj.SetGender)
        self.assertRaises(TypeError, self.obj.SetGender, 1, 2)
        self.assertRaises(TypeError, self.obj.SetGender, x=1)

        self.obj.SetGender(Atrinik.Gender.MALE)
        self.assertEqual(self.obj.GetGender(), Atrinik.Gender.MALE)
        self.assertTrue(self.obj.f_is_male)
        self.assertFalse(self.obj.f_is_female)

        self.obj.SetGender(Atrinik.Gender.FEMALE)
        self.assertEqual(self.obj.GetGender(), Atrinik.Gender.FEMALE)
        self.assertTrue(self.obj.f_is_female)
        self.assertFalse(self.obj.f_is_male)

        self.obj.SetGender(Atrinik.Gender.NEUTER)
        self.assertEqual(self.obj.GetGender(), Atrinik.Gender.NEUTER)
        self.assertFalse(self.obj.f_is_male)
        self.assertFalse(self.obj.f_is_female)

        self.obj.SetGender(Atrinik.Gender.HERMAPHRODITE)
        self.assertEqual(self.obj.GetGender(), Atrinik.Gender.HERMAPHRODITE)
        self.assertTrue(self.obj.f_is_male)
        self.assertTrue(self.obj.f_is_female)

    def test_Update(self):
        self.assertRaises(TypeError, self.obj.Update, 1, 2)
        self.assertRaises(TypeError, self.obj.Update, x=1)

        ring = activator.CreateObject("ring_generic")
        ring.Str = 1
        strength = activator.Str
        activator.f_no_fix_player = True
        activator.Apply(ring)
        activator.f_no_fix_player = False
        self.assertEqual(activator.Str, strength)
        activator.Update()
        self.assertEqual(activator.Str, strength + 1)

    def test_Hit(self):
        self.assertRaises(TypeError, self.obj.Hit)
        self.assertRaises(TypeError, self.obj.Hit, 1, 2)
        self.assertRaises(TypeError, self.obj.Hit, x=1)

        m = Atrinik.CreateMap(5, 5, "test-atrinik-object-hit")
        m.Insert(activator, 3, 3)

        hp = activator.hp
        activator.Hit(activator, 10)
        self.assertEqual(activator.hp, hp)

        raas = m.CreateObject("raas", 4, 4)
        hp = activator.hp
        raas.Hit(activator, 10)
        self.assertLess(activator.hp, hp)

        self.assertRaises(ValueError, activator.Hit, self.obj, 1)
        self.assertRaises(ValueError, raas.Hit, self.obj, 1)

        raas2 = Atrinik.CreateObject("raas")
        self.assertRaises(ValueError, raas.Hit, raas2, 1)
        raas2.Destroy()

        self.assertTrue(raas)
        activator.Hit(raas, -1)
        self.assertFalse(raas)

    def test_Cast(self):
        self.assertRaises(TypeError, self.obj.Cast)
        self.assertRaises(TypeError, self.obj.Cast, 1, 2)
        self.assertRaises(TypeError, self.obj.Cast, x=1)

        m = Atrinik.CreateMap(5, 5, "test-atrinik-object-cast")
        m.Insert(activator, 0, 0)

        raas = m.CreateObject("raas", 1, 1)
        raas.maxhp = 1
        raas.Update()
        self.assertTrue(raas)

        activator.direction = Atrinik.SOUTHEAST
        activator.Cast(Atrinik.GetArchetype("spell_firestorm").clone.sp)

        for _ in range(5):
            Atrinik.Process()

        self.assertFalse(raas)


activator = Atrinik.WhoIsActivator()
me = Atrinik.WhoAmI()
suites = [
    unittest.TestLoader().loadTestsFromTestCase(ObjectTestCase)
]
