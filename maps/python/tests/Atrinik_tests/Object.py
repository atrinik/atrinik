import random
import unittest

import Atrinik
from tests import simulate_server


class ObjectTestCase(unittest.TestCase):
    maxDiff = None

    def setUp(self):
        simulate_server(count=1, wait=False)
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

        activator.hp = activator.maxhp
        hp = activator.hp
        activator.Hit(activator, 10)
        self.assertEqual(activator.hp, hp)

        raas = m.CreateObject("raas", 4, 4)
        raas.Update()
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

        simulate_server(count=5, wait=False)
        self.assertFalse(raas)

        activator.sp = 500
        activator.Cast(Atrinik.GetArchetype("spell_create_food").clone.sp,
                       direction=Atrinik.EAST)

        obj = m.Objects(activator.x + Atrinik.freearr_x[Atrinik.EAST],
                        activator.y + Atrinik.freearr_y[Atrinik.EAST])[0]
        self.assertIsNotNone(obj)
        self.assertIn(obj.type, (Atrinik.Type.DRINK, Atrinik.Type.FOOD))
        obj.Destroy()

        activator.sp = 500
        activator.Cast(Atrinik.GetArchetype("spell_create_food").clone.sp,
                       direction=Atrinik.EAST, option="pipeweed")
        obj = m.Objects(activator.x + Atrinik.freearr_x[Atrinik.EAST],
                        activator.y + Atrinik.freearr_y[Atrinik.EAST])[0]
        self.assertIsNotNone(obj)
        self.assertIn(obj.type, (Atrinik.Type.DRINK, Atrinik.Type.FOOD))
        self.assertEqual(obj.arch.name, "pipeweed")
        obj.Destroy()

        raas1 = m.CreateObject("raas", 2, 2)
        raas1.Update()

        raas2 = m.CreateObject("raas", 3, 3)
        raas2.Update()

        raas1.hp = raas1.maxhp - 1
        self.assertLess(raas1.hp, raas1.maxhp)
        raas2.Cast(Atrinik.GetArchetype("spell_minor_healing").clone.sp,
                   target=raas1)
        self.assertEqual(raas1.hp, raas1.maxhp)

        raas1.Destroy()
        raas2.Destroy()

    def test_CreateForce(self):
        self.assertRaises(TypeError, self.obj.CreateForce)
        self.assertRaises(TypeError, self.obj.CreateForce, 1, 2)
        self.assertRaises(TypeError, self.obj.CreateForce, x=1)

        ac_force = activator.CreateForce("test-create-force")
        self.assertEqual(ac_force.name, "test-create-force")
        self.assertAlmostEqual(ac_force.speed, 0.0)
        self.assertFalse(ac_force.f_is_used_up)
        self.assertEqual(activator.FindObject(type=Atrinik.Type.FORCE),
                         ac_force)

        force = self.obj.CreateForce("test-create-force", expiration=1)
        self.assertEqual(force.name, "test-create-force")
        self.assertAlmostEqual(force.speed, 0.02)
        self.assertEquals(force.food, 1)
        self.assertTrue(force.f_is_used_up)
        self.assertEqual(self.obj.FindObject(type=Atrinik.Type.FORCE), force)
        self.assertTrue(force)
        verify = lambda obj: self.assertTrue(force)
        simulate_server(count=49, wait=False, before_cb=verify, after_cb=verify,
                        obj=force)
        simulate_server(count=1, wait=False)
        self.assertFalse(force)

        force = self.obj.CreateForce("test-create-force", seconds=1.5)
        self.assertEqual(force.name, "test-create-force")
        self.assertGreater(force.speed, 0.0)
        self.assertTrue(force.f_is_used_up)
        self.assertEqual(self.obj.FindObject(type=Atrinik.Type.FORCE), force)
        verify = lambda obj: self.assertTrue(force)
        simulate_server(seconds=1.0, before_cb=verify, after_cb=verify,
                        obj=force)
        simulate_server(seconds=1.0)
        self.assertFalse(force)

        force = self.obj.CreateForce("test-create-force", seconds=15.5)
        self.assertEqual(force.name, "test-create-force")
        self.assertGreater(force.speed, 0.0)
        self.assertTrue(force.f_is_used_up)
        self.assertEquals(force.food, 1)
        self.assertEqual(self.obj.FindObject(type=Atrinik.Type.FORCE), force)
        self.assertTrue(force)
        verify = lambda obj: self.assertTrue(force)
        iterations = 15 * 1000000 / Atrinik.MAX_TIME
        simulate_server(count=iterations, wait=False, before_cb=verify,
                        after_cb=verify, obj=force)
        simulate_server(seconds=1.0)
        self.assertFalse(force)

        force = self.obj.CreateForce("test-create-force", seconds=28)
        self.assertEqual(force.name, "test-create-force")
        self.assertGreater(force.speed, 0.0)
        self.assertTrue(force.f_is_used_up)
        self.assertEquals(force.food, 2)
        self.assertEqual(self.obj.FindObject(type=Atrinik.Type.FORCE), force)
        self.assertTrue(force)
        verify = lambda obj: self.assertTrue(force)
        iterations = 27 * 1000000 / Atrinik.MAX_TIME
        simulate_server(count=iterations, wait=False, before_cb=verify,
                        after_cb=verify, obj=force)
        simulate_server(seconds=1.5)
        self.assertFalse(force)

        self.assertTrue(ac_force)
        ac_force.Destroy()

    def test_CreateObject(self):
        self.assertRaises(TypeError, self.obj.CreateObject)
        self.assertRaises(TypeError, self.obj.CreateObject, 1, 2)
        self.assertRaises(TypeError, self.obj.CreateObject, x=1)

        obj = self.obj.CreateObject("sword")
        self.assertIsNotNone(obj)
        self.assertEqual(obj.arch.name, "sword")
        self.assertEqual(obj.nrof, 0)
        self.assertEqual(obj.env, self.obj)
        self.assertEqual(obj.value, obj.arch.clone.value)
        self.assertTrue(obj.f_identified)
        self.assertEqual(self.obj.inv[0], obj)
        obj.Destroy()

        obj = self.obj.CreateObject("scroll_generic", nrof=50)
        self.assertIsNotNone(obj)
        self.assertEqual(obj.arch.name, "scroll_generic")
        self.assertEqual(obj.nrof, 50)
        self.assertEqual(obj.env, self.obj)
        self.assertEqual(obj.value, obj.arch.clone.value)
        self.assertTrue(obj.f_identified)
        self.assertEqual(self.obj.inv[0], obj)
        obj.Destroy()

        obj = self.obj.CreateObject("crystal_light", nrof=3, identified=False)
        self.assertIsNotNone(obj)
        self.assertEqual(obj.arch.name, "crystal_light")
        self.assertEqual(obj.nrof, 3)
        self.assertEqual(obj.env, self.obj)
        self.assertEqual(obj.value, obj.arch.clone.value)
        self.assertFalse(obj.f_identified)
        self.assertEqual(self.obj.inv[0], obj)
        obj.Destroy()

        obj = self.obj.CreateObject("sack", value=0)
        self.assertIsNotNone(obj)
        self.assertEqual(obj.arch.name, "sack")
        self.assertEqual(obj.nrof, 0)
        self.assertEqual(obj.env, self.obj)
        self.assertEqual(obj.value, 0)
        self.assertTrue(obj.f_identified)
        self.assertEqual(self.obj.inv[0], obj)
        obj.Destroy()

    def setup_FindObject(self):
        self.objs = {}
        self.sack = self.obj.CreateObject("sack")

        for i in range(50):
            key = "torch{0:02d}".format(i)
            where = self.obj if i < 30 else self.sack
            self.objs[key] = where.CreateObject("torch")

        self.objs["torch05"].f_unpaid = True
        self.objs["torch10"].f_unpaid = True
        self.objs["torch10"].title = "light"
        self.objs["torch15"].title = "light"
        self.objs["torch20"].type = Atrinik.Type.MISC_OBJECT
        self.objs["torch21"].type = Atrinik.Type.MISC_OBJECT
        self.objs["torch21"].f_unpaid = True
        self.objs["torch22"].type = Atrinik.Type.MISC_OBJECT
        self.objs["torch22"].title = "light"
        self.objs["torch22"].name = "not torch"
        self.objs["torch30"].f_unpaid = True
        self.objs["torch30"].title = "light"
        self.objs["torch31"].f_unpaid = True
        self.objs["torch32"].type = Atrinik.Type.MISC_OBJECT
        self.objs["torch40"].type = Atrinik.Type.WEAPON
        self.objs["torch41"].type = Atrinik.Type.WEAPON
        self.objs["torch49"].f_unpaid = True

    def test_FindObject(self):
        self.assertRaises(TypeError, self.obj.FindObject, 1, 2)
        self.assertRaises(TypeError, self.obj.FindObject, x=1)

        self.assertRaises(ValueError, self.obj.FindObject)

        self.assertIsNone(self.obj.FindObject(archname="torch"))
        self.assertIsNone(self.obj.FindObject(name="torch"))
        self.assertIsNone(self.obj.FindObject(title=""))
        self.assertIsNone(self.obj.FindObject(title="light"))
        self.assertIsNone(self.obj.FindObject(type=Atrinik.Type.LIGHT_APPLY))
        self.assertIsNone(self.obj.FindObject(unpaid=True))

        self.setup_FindObject()

        self.assertEqual(self.obj.FindObject(archname="torch"),
                         self.objs["torch29"])
        self.assertEqual(self.obj.FindObject(archname="torch"),
                         self.objs["torch29"])
        self.assertEqual(self.obj.FindObject(name="torch"),
                         self.objs["torch29"])
        self.assertEqual(self.obj.FindObject(archname="torch", name="torch"),
                         self.objs["torch29"])

        self.assertEqual(self.obj.FindObject(archname="torch", title="light",
                                             type=Atrinik.Type.LIGHT_APPLY),
                         self.objs["torch15"])
        self.assertEqual(self.obj.FindObject(title="light",
                                             type=Atrinik.Type.LIGHT_APPLY),
                         self.objs["torch15"])

        self.assertEqual(self.obj.FindObject(archname="torch",
                                             name="not torch"),
                         self.objs["torch22"])
        self.assertEqual(self.obj.FindObject(archname="torch", title="light",
                                             name="not torch"),
                         self.objs["torch22"])
        self.assertEqual(self.obj.FindObject(archname="torch", title="light",
                                             name="not torch",
                                             type=Atrinik.Type.MISC_OBJECT),
                         self.objs["torch22"])
        self.assertEqual(self.obj.FindObject(title="light",
                                             name="not torch",
                                             type=Atrinik.Type.MISC_OBJECT),
                         self.objs["torch22"])
        self.assertEqual(self.obj.FindObject(name="not torch",
                                             type=Atrinik.Type.MISC_OBJECT),
                         self.objs["torch22"])
        self.assertEqual(self.obj.FindObject(type=Atrinik.Type.MISC_OBJECT),
                         self.objs["torch22"])

        self.assertIsNone(self.obj.FindObject(type=Atrinik.Type.WEAPON))
        self.assertEqual(self.obj.FindObject(mode=Atrinik.INVENTORY_CONTAINERS,
                                             type=Atrinik.Type.WEAPON),
                         self.objs["torch41"])
        self.assertEqual(self.obj.FindObject(mode=Atrinik.INVENTORY_ALL,
                                             type=Atrinik.Type.WEAPON),
                         self.objs["torch41"])

        self.assertEqual(self.sack.FindObject(name="torch"),
                         self.objs["torch49"])

    def test_FindObjects(self):
        self.assertRaises(TypeError, self.obj.FindObjects, 1, 2)
        self.assertRaises(TypeError, self.obj.FindObjects, x=1)

        self.assertRaises(ValueError, self.obj.FindObjects)

        l = self.obj.FindObjects(archname="torch")
        self.assertFalse(l)
        self.assertIsInstance(l, list)
        self.assertEqual(len(l), 0)
        l = self.obj.FindObjects(name="torch")
        self.assertFalse(l)
        self.assertIsInstance(l, list)
        self.assertEqual(len(l), 0)
        l = self.obj.FindObjects(title="")
        self.assertFalse(l)
        self.assertIsInstance(l, list)
        self.assertEqual(len(l), 0)
        l = self.obj.FindObjects(title="light")
        self.assertFalse(l)
        self.assertIsInstance(l, list)
        self.assertEqual(len(l), 0)
        l = self.obj.FindObjects(type=Atrinik.Type.LIGHT_APPLY)
        self.assertFalse(l)
        self.assertIsInstance(l, list)
        self.assertEqual(len(l), 0)
        l = self.obj.FindObjects(unpaid=True)
        self.assertFalse(l)
        self.assertIsInstance(l, list)
        self.assertEqual(len(l), 0)

        self.setup_FindObject()

        expected = []
        for obj in self.obj.inv:
            if obj.f_unpaid:
                expected.append(obj)

        self.assertEqual(self.obj.FindObjects(archname="torch", unpaid=True),
                         expected)

        expected = []
        for obj in list(self.obj.inv) + list(self.sack.inv):
            if obj.f_unpaid:
                expected.append(obj)

        self.assertEqual(self.obj.FindObjects(mode=Atrinik.INVENTORY_ALL,
                                              archname="torch", unpaid=True),
                         expected)
        self.assertEqual(self.obj.FindObjects(mode=Atrinik.INVENTORY_ALL,
                                              name="torch", unpaid=True),
                         expected)

        expected = []
        for obj in list(self.obj.inv) + list(self.sack.inv):
            if obj.name == "torch":
                expected.append(obj)

        self.assertEqual(self.obj.FindObjects(mode=Atrinik.INVENTORY_ALL,
                                              name="torch"),
                         expected)

        expected = []
        for obj in list(self.obj.inv) + list(self.sack.inv):
            if obj.arch.name == "torch":
                expected.append(obj)
        self.assertEqual(self.obj.FindObjects(mode=Atrinik.INVENTORY_ALL,
                                              archname="torch"),
                         expected)

        expected = []
        for obj in list(self.obj.inv) + list(self.sack.inv):
            if obj.type == Atrinik.Type.WEAPON:
                expected.append(obj)

        self.assertEqual(self.obj.FindObjects(mode=Atrinik.INVENTORY_ALL,
                                              type=Atrinik.Type.WEAPON),
                         expected)

        expected = []
        for obj in list(self.obj.inv) + list(self.sack.inv):
            if obj.title == "light":
                expected.append(obj)

        self.assertEqual(self.obj.FindObjects(mode=Atrinik.INVENTORY_ALL,
                                              archname="torch", title="light"),
                         expected)
        self.assertEqual(self.obj.FindObjects(mode=Atrinik.INVENTORY_ALL,
                                              title="light"),
                         expected)

        expected = []
        for obj in list(self.obj.inv) + list(self.sack.inv):
            if obj.title == "light" and obj.name == "torch":
                expected.append(obj)

        self.assertEqual(self.obj.FindObjects(mode=Atrinik.INVENTORY_ALL,
                                              name="torch", title="light"),
                         expected)
        self.assertEqual(self.obj.FindObjects(mode=Atrinik.INVENTORY_ALL,
                                              archname="torch",  name="torch",
                                              title="light"),
                         expected)

        expected = []
        for obj in list(self.obj.inv) + list(self.sack.inv):
            if obj.title == "light" and obj.name == "not torch":
                expected.append(obj)

        self.assertEqual(self.obj.FindObjects(mode=Atrinik.INVENTORY_ALL,
                                              name="not torch", title="light"),
                         expected)
        self.assertEqual(self.obj.FindObjects(mode=Atrinik.INVENTORY_ALL,
                                              name="not torch",
                                              archname="torch", title="light"),
                         expected)

        expected = []
        for obj in list(self.sack.inv):
            if obj.type == Atrinik.Type.WEAPON:
                expected.append(obj)

        self.assertEqual(self.obj.FindObjects(mode=Atrinik.INVENTORY_ALL,
                                              archname="torch",
                                              type=Atrinik.Type.WEAPON),
                         expected)
        self.assertEqual(self.obj.FindObjects(mode=Atrinik.INVENTORY_ALL,
                                              name="torch",
                                              type=Atrinik.Type.WEAPON),
                         expected)
        self.assertEqual(self.obj.FindObjects(mode=Atrinik.INVENTORY_ALL,
                                              type=Atrinik.Type.WEAPON),
                         expected)


activator = Atrinik.WhoIsActivator()
me = Atrinik.WhoAmI()
suites = [
    unittest.TestLoader().loadTestsFromTestCase(ObjectTestCase)
]
