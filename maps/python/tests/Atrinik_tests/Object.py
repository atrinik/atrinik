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

    def test_GetGender(self):
        self.assertRaises(TypeError, self.obj.GetGender, 1, 2)
        self.assertRaises(TypeError, self.obj.GetGender, x=1)

        self.assertEqual(self.obj.GetGender(), Atrinik.Gender.NEUTER)
        self.obj.f_is_male = True
        self.assertEqual(self.obj.GetGender(), Atrinik.Gender.MALE)
        self.obj.f_is_female = True
        self.assertEqual(self.obj.GetGender(), Atrinik.Gender.HERMAPHRODITE)
        self.obj.f_is_male = False
        self.assertEqual(self.obj.GetGender(), Atrinik.Gender.FEMALE)

    def test_SetGender(self):
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

    def test_Remove(self):
        self.assertRaises(TypeError, self.obj.Remove, 1, 2)
        self.assertRaises(TypeError, self.obj.Remove, x=1)

        self.assertRaises(Atrinik.AtrinikError, self.obj.Remove)

        m = Atrinik.CreateMap(5, 5, "test-atrinik-object-remove")
        m.Insert(self.obj, 0, 0)
        self.obj.Remove()
        self.assertFalse(m.Objects(0, 0))
        self.assertEqual(len(m.Objects(0, 0)), 0)

        self.obj.InsertInto(activator)
        self.assertEqual(activator.inv[0], self.obj)
        self.obj.Remove()
        self.assertNotEqual(activator.inv[0], self.obj)

    def test_Destroy(self):
        self.assertRaises(TypeError, self.obj.Destroy, 1, 2)
        self.assertRaises(TypeError, self.obj.Destroy, x=1)

        obj = Atrinik.CreateObject("sword")
        obj.Destroy()
        self.assertRaises(ReferenceError, obj.Destroy)

        m = Atrinik.CreateMap(5, 5, "test-atrinik-object-remove")
        obj = m.CreateObject("sword", 0, 0)
        obj.Destroy()
        self.assertRaises(ReferenceError, obj.Destroy)
        self.assertFalse(m.Objects(0, 0))
        self.assertEqual(len(m.Objects(0, 0)), 0)

        orig = activator.inv[0]
        obj = activator.CreateObject("sword")
        self.assertEqual(activator.inv[0], obj)
        obj.Destroy()
        self.assertRaises(ReferenceError, obj.Destroy)
        self.assertEqual(activator.inv[0], orig)

    def test_SetPosition(self):
        self.assertRaises(TypeError, self.obj.SetPosition, 1, "2")
        self.assertRaises(TypeError, self.obj.SetPosition, x=1)

        m = Atrinik.CreateMap(5, 5, "test-atrinik-object-set-position")
        m.Insert(self.obj, 0, 0)
        self.assertTrue(m.Objects(0, 0))
        self.assertEqual(len(m.Objects(0, 0)), 1)
        self.assertEqual(m.Objects(0, 0)[0], self.obj)
        self.obj.SetPosition(0, 0)
        self.assertTrue(m.Objects(0, 0))
        self.assertEqual(len(m.Objects(0, 0)), 1)
        self.assertEqual(m.Objects(0, 0)[0], self.obj)
        self.obj.SetPosition(1, 0)
        self.assertFalse(m.Objects(0, 0))
        self.assertEqual(len(m.Objects(0, 0)), 0)
        self.assertTrue(m.Objects(1, 0))
        self.assertEqual(len(m.Objects(1, 0)), 1)
        self.assertEqual(m.Objects(1, 0)[0], self.obj)
        self.obj.SetPosition(4, 4)
        self.assertFalse(m.Objects(1, 0))
        self.assertEqual(len(m.Objects(1, 0)), 0)
        self.assertTrue(m.Objects(4, 4))
        self.assertEqual(len(m.Objects(4, 4)), 1)
        self.assertEqual(m.Objects(4, 4)[0], self.obj)
        self.obj.Remove()
        self.assertFalse(m.Objects(4, 4))
        self.assertEqual(len(m.Objects(4, 4)), 0)

    def test_CastIdentify(self):
        self.assertRaises(TypeError, self.obj.CastIdentify)
        self.assertRaises(TypeError, self.obj.CastIdentify, 1, 2)
        self.assertRaises(TypeError, self.obj.CastIdentify, x=1)

        m = Atrinik.CreateMap(5, 5, "test-atrinik-object-cast-identify")
        m.Insert(activator, 0, 0)
        npc = m.CreateObject("smith", 1, 1)
        npc.Update()

        sword = activator.CreateObject("sword", identified=False)
        sword.title = "of lordliness"
        wand = activator.CreateObject("wand", identified=False)
        wand.level = 10
        wand.sp = 5
        sack = activator.CreateObject("sack", identified=False)
        sword2 = sack.CreateObject("sword", identified=False)
        sword2.title = "of the General"
        torch = sack.CreateObject("torch", identified=False)
        torch.name = "ultimate torch"
        torch.title = "of ultimates"
        torch.msg = "Truly the ultimate torch, passed down from the ancients."

        activator.Controller().s_packets.clear()
        self.assertRaises(Atrinik.AtrinikError, npc.CastIdentify, activator,
                          Atrinik.IDENTIFY_MARKED)
        self.assertFalse(sword.f_identified)
        self.assertFalse(wand.f_identified)
        self.assertFalse(sack.f_identified)
        self.assertFalse(sword2.f_identified)
        self.assertFalse(torch.f_identified)
        packets = b"".join(activator.Controller().s_packets)
        self.assertEqual(len(packets), 0)

        npc.CastIdentify(activator, Atrinik.IDENTIFY_MARKED, sack)
        self.assertTrue(sack.f_identified)
        self.assertFalse(sword.f_identified)
        self.assertFalse(wand.f_identified)
        self.assertFalse(sword2.f_identified)
        self.assertFalse(torch.f_identified)
        packets = b"".join(activator.Controller().s_packets)
        self.assertIn(sack.GetName(activator).encode(), packets)

        activator.Controller().s_packets.clear()
        npc.CastIdentify(activator, Atrinik.IDENTIFY_ALL, sack)
        self.assertTrue(sack.f_identified)
        self.assertTrue(sword2.f_identified)
        self.assertTrue(torch.f_identified)
        self.assertFalse(sword.f_identified)
        self.assertFalse(wand.f_identified)
        packets = b"".join(activator.Controller().s_packets)
        self.assertIn(torch.GetName(activator).encode(), packets)
        self.assertIn(sword2.GetName(activator).encode(), packets)
        self.assertIn(b"The item has a story:", packets)
        self.assertIn(torch.msg.encode(), packets)

        sword.f_identified = False
        wand.f_identified = False
        sack.f_identified = False
        sword2.f_identified = False
        torch.f_identified = False

        activator.Controller().s_packets.clear()
        npc.CastIdentify(activator, Atrinik.IDENTIFY_ALL)
        self.assertTrue(sack.f_identified)
        self.assertTrue(sword.f_identified)
        self.assertFalse(wand.f_identified)
        self.assertFalse(sword2.f_identified)
        self.assertFalse(torch.f_identified)
        packets = b"".join(activator.Controller().s_packets)
        self.assertIn(sack.GetName(activator).encode(), packets)
        self.assertIn(sword.GetName(activator).encode(), packets)

        npc.level = 20
        npc.Update()
        activator.Controller().s_packets.clear()
        npc.CastIdentify(activator, Atrinik.IDENTIFY_ALL)
        self.assertTrue(sack.f_identified)
        self.assertTrue(sword.f_identified)
        self.assertTrue(wand.f_identified)
        self.assertFalse(sword2.f_identified)
        self.assertFalse(torch.f_identified)
        packets = b"".join(activator.Controller().s_packets)
        self.assertIn(wand.GetName(activator).encode(), packets)

    def test_Save(self):
        self.assertRaises(TypeError, self.obj.Save, 1, 2)
        self.assertRaises(TypeError, self.obj.Save, x=1)

        s = self.obj.Save()
        self.assertTrue(s.startswith("arch sword\n"))
        self.assertTrue(s.endswith("end\n"))
        self.assertEqual(s, "arch sword\nend\n")

        self.obj.f_identified = True
        s = self.obj.Save()
        self.assertTrue(s.startswith("arch sword\n"))
        self.assertTrue(s.endswith("end\n"))
        self.assertEqual(s, "arch sword\nidentified 1\nend\n")

    def test_GetCost(self):
        self.assertRaises(TypeError, self.obj.GetCost, 1, 2)
        self.assertRaises(TypeError, self.obj.GetCost, x=1)

        self.obj.f_identified = True
        self.assertEqual(self.obj.GetCost(), self.obj.value)
        self.assertEqual(self.obj.GetCost(Atrinik.COST_BUY), self.obj.value)
        self.assertEqual(self.obj.GetCost(Atrinik.COST_SELL),
                         self.obj.value * 0.2)

    def test_GetMoney(self):
        self.assertRaises(TypeError, self.obj.GetMoney, 1, 2)
        self.assertRaises(TypeError, self.obj.GetMoney, x=1)

        self.assertEqual(activator.GetMoney(), 0)
        money = activator.CreateObject("goldcoin")
        money.nrof = 50
        self.assertEqual(activator.GetMoney(), money.value * money.nrof)
        money.Destroy()

    def test_PayAmount(self):
        self.assertRaises(TypeError, self.obj.PayAmount, 1, 2)
        self.assertRaises(TypeError, self.obj.PayAmount, x=1)

        self.assertTrue(activator.PayAmount(0))
        self.assertFalse(activator.PayAmount(1))
        money = activator.CreateObject("goldcoin")
        money.nrof = 50
        self.assertFalse(activator.PayAmount(money.value * money.nrof + 1))
        self.assertTrue(activator.PayAmount(money.value * money.nrof))
        self.assertTrue(activator.PayAmount(0))
        self.assertFalse(activator.PayAmount(1))

    def test_Clone(self):
        self.assertRaises(TypeError, self.obj.Clone, 1, 2)
        self.assertRaises(TypeError, self.obj.Clone, x=1)

        no_inv_obj = self.obj.Save()

        clone = self.obj.Clone()
        self.assertNotEqual(self.obj, clone)
        self.assertEqual(self.obj.name, clone.name)
        self.assertEqual(self.obj.arch.name, clone.arch.name)
        self.assertEqual(no_inv_obj, clone.Save())
        self.assertFalse(clone.inv)
        clone.Destroy()

        clone = self.obj.Clone(False)
        self.assertNotEqual(self.obj, clone)
        self.assertEqual(self.obj.name, clone.name)
        self.assertEqual(self.obj.arch.name, clone.arch.name)
        self.assertEqual(no_inv_obj, clone.Save())
        self.assertFalse(clone.inv)
        clone.Destroy()

        torch = self.obj.CreateObject("torch")
        self.assertEqual(self.obj.inv[0], torch)

        clone = self.obj.Clone()
        self.assertNotEqual(self.obj, clone)
        self.assertEqual(self.obj.name, clone.name)
        self.assertEqual(self.obj.arch.name, clone.arch.name)
        self.assertEqual(self.obj.Save(), clone.Save())
        self.assertTrue(clone.inv[0])
        self.assertNotEqual(self.obj.inv[0], clone.inv[0])
        self.assertEqual(self.obj.inv[0].name, clone.inv[0].name)
        self.assertEqual(self.obj.inv[0].arch.name, clone.inv[0].arch.name)
        self.assertEqual(self.obj.inv[0].Save(), clone.inv[0].Save())
        clone.Destroy()

        clone = self.obj.Clone(False)
        self.assertNotEqual(self.obj, clone)
        self.assertEqual(self.obj.name, clone.name)
        self.assertEqual(self.obj.arch.name, clone.arch.name)
        self.assertEqual(no_inv_obj, clone.Save())
        self.assertFalse(clone.inv)
        clone.Destroy()

    def test_ReadKey(self):
        self.assertRaises(TypeError, self.obj.ReadKey)
        self.assertRaises(TypeError, self.obj.ReadKey, 1, 2)
        self.assertRaises(TypeError, self.obj.ReadKey, x=1)

        self.assertIsNone(self.obj.ReadKey("xxx"))
        self.assertIsNone(self.obj.ReadKey("identified"))
        self.obj.f_identified = True
        self.assertIsNone(self.obj.ReadKey("identified"))
        self.obj.f_identified = False

        self.obj.WriteKey("identified", "1")
        self.assertEqual(self.obj.ReadKey("identified"), "1")
        self.assertIsNone(self.obj.ReadKey("xxx"))
        self.assertFalse(self.obj.f_identified)

        self.obj.WriteKey("xxx", "hello world")
        self.assertEqual(self.obj.ReadKey("identified"), "1")
        self.assertEqual(self.obj.ReadKey("xxx"), "hello world")
        self.assertIsNone(self.obj.ReadKey("yyy"))
        self.assertFalse(self.obj.f_identified)

    def test_WriteKey(self):
        self.assertRaises(TypeError, self.obj.WriteKey)
        self.assertRaises(TypeError, self.obj.WriteKey, 1, 2)
        self.assertRaises(TypeError, self.obj.WriteKey, x=1)

        self.obj.WriteKey("xxx", "hello world")
        self.assertEqual(self.obj.ReadKey("xxx"), "hello world")
        self.obj.WriteKey("xxx")
        self.assertIsNone(self.obj.ReadKey("xxx"))
        self.obj.WriteKey("xxx", "hello world", False)
        self.assertIsNone(self.obj.ReadKey("xxx"))
        self.obj.WriteKey("xxx", "hello world")
        self.assertEqual(self.obj.ReadKey("xxx"), "hello world")
        self.obj.WriteKey("xxx", "hello horizons", False)
        self.assertEqual(self.obj.ReadKey("xxx"), "hello horizons")
        self.obj.WriteKey("yyy", "hello pluto")
        self.assertEqual(self.obj.ReadKey("yyy"), "hello pluto")
        self.assertEqual(self.obj.ReadKey("xxx"), "hello horizons")
        self.assertIsNone(self.obj.ReadKey("zzz"))
        self.obj.WriteKey("yyy")
        self.assertEqual(self.obj.ReadKey("xxx"), "hello horizons")
        self.assertIsNone(self.obj.ReadKey("yyy"))
        self.obj.WriteKey("xxx")
        self.assertIsNone(self.obj.ReadKey("yyy"))
        self.assertIsNone(self.obj.ReadKey("xxx"))

    def test_GetName(self):
        self.assertRaises(TypeError, self.obj.GetName, 1, 2)
        self.assertRaises(TypeError, self.obj.GetName, x=1)

        self.assertEqual(me.GetName(), me.name)
        self.assertEqual(activator.GetName(), activator.name)

        self.assertEqual(self.obj.GetName(), "sword")
        self.assertEqual(self.obj.GetName(activator), "sword")
        self.obj.f_identified = True
        self.assertEqual(self.obj.GetName(), "iron sword")
        self.assertEqual(self.obj.GetName(activator), "iron sword")

        torch = activator.CreateObject("torch")
        self.assertEqual(torch.GetName(), "torch")
        self.assertEqual(torch.GetName(activator), "torch")
        torch.nrof = 50
        self.assertEqual(torch.GetName(), "50 torch")
        self.assertEqual(torch.GetName(activator), "50 torch")
        torch.Destroy()

        corpse = activator.CreateObject("corpse_default")
        corpse.sub_type = 65
        self.assertEqual(corpse.GetName(), "decaying corpse")
        self.assertEqual(corpse.GetName(activator), "decaying corpse")
        corpse.slaying = me.name
        self.assertEqual(corpse.GetName(), "decaying corpse (bounty of " +
                                           me.name + ")")
        self.assertEqual(corpse.GetName(activator), "decaying corpse (bounty "
                                                    "of " + me.name + ")")

        corpse.sub_type = 129
        corpse.slaying = "test"
        self.assertEqual(corpse.GetName(), "decaying corpse (bounty of a "
                                           "party)")
        self.assertEqual(corpse.GetName(activator), "decaying corpse (bounty "
                                                    "of another party)")
        activator.Controller().ExecuteCommand("/party form test")
        self.assertEqual(corpse.GetName(), "decaying corpse (bounty of a "
                                           "party)")
        self.assertEqual(corpse.GetName(activator), "decaying corpse (bounty "
                                                    "of your party)")
        activator.Controller().ExecuteCommand("/party leave")

    def test_Controller(self):
        self.assertRaises(TypeError, self.obj.Controller, 1, 2)
        self.assertRaises(TypeError, self.obj.Controller, x=1)

        self.assertRaises(Atrinik.AtrinikError, self.obj.Controller)

        self.assertIsNotNone(activator.Controller())
        self.assertIsInstance(activator.Controller(), Atrinik.Player.Player)
        self.assertEqual(activator.Controller().ob, activator)

    def test_Protection(self):
        self.assertRaises(TypeError, self.obj.Protection)
        self.assertRaises(TypeError, self.obj.Protection, 1, 2)
        self.assertRaises(TypeError, self.obj.Protection, x=1)

        self.assertRaises(IndexError, self.obj.Protection, -1)
        self.assertRaises(IndexError, self.obj.Protection, 5000)

        self.assertEqual(self.obj.Protection(Atrinik.ATNR_SLASH), 0)
        self.obj.SetProtection(Atrinik.ATNR_SLASH, 50)
        self.assertEqual(self.obj.Protection(Atrinik.ATNR_SLASH), 50)

    def test_SetProtection(self):
        self.assertRaises(TypeError, self.obj.SetProtection)
        self.assertRaises(TypeError, self.obj.SetProtection, 1, "2")
        self.assertRaises(TypeError, self.obj.SetProtection, x=1)

        self.assertRaises(IndexError, self.obj.SetProtection, -1, 0)
        self.assertRaises(IndexError, self.obj.SetProtection, 5000, 0)

        self.assertRaises(OverflowError, self.obj.SetProtection,
                          Atrinik.ATNR_SLASH, 200)
        self.assertRaises(OverflowError, self.obj.SetProtection,
                          Atrinik.ATNR_SLASH, -200)

        self.obj.SetProtection(Atrinik.ATNR_SLASH, 50)
        self.assertEqual(self.obj.Protection(Atrinik.ATNR_SLASH), 50)

        self.obj.SetProtection(Atrinik.ATNR_SLASH, 127)
        self.assertEqual(self.obj.Protection(Atrinik.ATNR_SLASH), 127)

        self.obj.SetProtection(Atrinik.ATNR_SLASH, -128)
        self.assertEqual(self.obj.Protection(Atrinik.ATNR_SLASH), -128)

    def test_Attack(self):
        self.assertRaises(TypeError, self.obj.Attack)
        self.assertRaises(TypeError, self.obj.Attack, 1, 2)
        self.assertRaises(TypeError, self.obj.Attack, x=1)

        self.assertRaises(IndexError, self.obj.Attack, -1)
        self.assertRaises(IndexError, self.obj.Attack, 5000)

        self.assertEqual(self.obj.Attack(Atrinik.ATNR_SLASH), 100)
        self.obj.SetAttack(Atrinik.ATNR_SLASH, 50)
        self.assertEqual(self.obj.Attack(Atrinik.ATNR_SLASH), 50)

    def test_SetAttack(self):
        self.assertRaises(TypeError, self.obj.SetAttack)
        self.assertRaises(TypeError, self.obj.SetAttack, 1, "2")
        self.assertRaises(TypeError, self.obj.SetAttack, x=1)

        self.assertRaises(IndexError, self.obj.SetAttack, -1, 0)
        self.assertRaises(IndexError, self.obj.SetAttack, 5000, 0)

        self.assertRaises(OverflowError, self.obj.SetAttack,
                          Atrinik.ATNR_SLASH, 300)
        self.assertRaises(OverflowError, self.obj.SetAttack,
                          Atrinik.ATNR_SLASH, -200)

        self.obj.SetAttack(Atrinik.ATNR_SLASH, 50)
        self.assertEqual(self.obj.Attack(Atrinik.ATNR_SLASH), 50)

        self.obj.SetAttack(Atrinik.ATNR_SLASH, 255)
        self.assertEqual(self.obj.Attack(Atrinik.ATNR_SLASH), 255)

        self.obj.SetAttack(Atrinik.ATNR_SLASH, 0)
        self.assertEqual(self.obj.Attack(Atrinik.ATNR_SLASH), 0)

    def test_Decrease(self):
        self.assertRaises(TypeError, self.obj.Decrease, 1, "2")
        self.assertRaises(TypeError, self.obj.Decrease, x=1)

        torch = activator.CreateObject("torch")
        torch.Decrease()
        self.assertFalse(torch)

        torch = activator.CreateObject("torch")
        torch.Decrease(50)
        self.assertFalse(torch)

        torch = activator.CreateObject("torch", nrof=50)
        torch.Decrease()
        self.assertTrue(torch)
        self.assertEqual(torch.nrof, 49)
        torch.Decrease(50)
        self.assertFalse(torch)

        torch = activator.CreateObject("torch", nrof=5)
        torch.Decrease()
        self.assertTrue(torch)
        self.assertEqual(torch.nrof, 4)
        torch.Decrease(4)
        self.assertFalse(torch)

    def test_SquaresAround(self):
        self.assertRaises(TypeError, self.obj.SquaresAround)
        self.assertRaises(TypeError, self.obj.SquaresAround, 1, "2")
        self.assertRaises(TypeError, self.obj.SquaresAround, x=1)

        self.assertRaises(ValueError, self.obj.SquaresAround, 0)
        self.assertRaises(TypeError, self.obj.SquaresAround, 1, callable=1)

        m = Atrinik.CreateMap(5, 5, "test-atrinik-object-squares-around")
        m.Insert(activator, 2, 2)

        l = []
        for i in range(Atrinik.SIZEOFFREE1):
            l.append((m, activator.x + Atrinik.freearr_x[i + 1],
                      activator.y + Atrinik.freearr_y[i + 1]))
        self.assertEqual(sorted(l), sorted(activator.SquaresAround(1)))

        l2 = activator.SquaresAround(range=1, type=Atrinik.AROUND_WALL)
        self.assertEqual(sorted(l), sorted(l2))

        m.CreateObject("blocked", 1, 1)
        l = []
        for i in range(Atrinik.SIZEOFFREE1):
            x = activator.x + Atrinik.freearr_x[i + 1]
            y = activator.y + Atrinik.freearr_y[i + 1]
            if x == 1 and y == 1:
                continue
            l.append((m, x, y))
        l2 = activator.SquaresAround(range=1, type=Atrinik.AROUND_WALL)
        self.assertEqual(sorted(l), sorted(l2))

        self.assertEqual(len(self.obj.SquaresAround(1)), 0)
        self.assertEqual(len(activator.SquaresAround(2)), 24)

        m.CreateObject("blocked", 1, 1)
        l = [(m, 1, 1)]
        fnc = lambda m2, x2, y2, obj: not m2.Objects(x2, y2)
        l2 = activator.SquaresAround(range=1, callable=fnc)
        self.assertEqual(sorted(l), sorted(l2))

    def test_GetRangeVector(self):
        self.assertRaises(TypeError, self.obj.GetRangeVector)
        self.assertRaises(TypeError, self.obj.GetRangeVector, 1, 2)
        self.assertRaises(TypeError, self.obj.GetRangeVector, x=1)

        m = Atrinik.CreateMap(50, 50, "test-atrinik-object-get-range-vector")
        m.Insert(self.obj, 0, 0)
        m.Insert(activator, 0, 0)
        t = self.obj.GetRangeVector(activator)
        self.assertIsInstance(t, tuple)
        self.assertEqual(t, (5, 0, 0, 0, self.obj))

        m.Insert(activator, 10, 10)
        t = self.obj.GetRangeVector(activator)
        self.assertIsInstance(t, tuple)
        self.assertEqual(t, (4, 14, 10, 10, self.obj))

        m.Insert(activator, 10, 10)
        t = activator.GetRangeVector(self.obj)
        self.assertIsInstance(t, tuple)
        self.assertEqual(t, (8, 14, -10, -10, activator))

    def test_CreateTreasure(self):
        self.assertRaises(TypeError, self.obj.CreateTreasure, 1, 2)
        self.assertRaises(TypeError, self.obj.CreateTreasure, x=1)

        self.assertRaises(ValueError, self.obj.CreateTreasure)

        self.obj.CreateTreasure("random_talisman")
        self.assertIn(self.obj.inv[0].type, (Atrinik.Type.AMULET,
                                             Atrinik.Type.RING))
        while self.obj.inv:
            self.obj.inv[0].Destroy()

        self.obj.randomitems = "random_talisman"
        self.obj.CreateTreasure()
        self.assertIn(self.obj.inv[0].type, (Atrinik.Type.AMULET,
                                             Atrinik.Type.RING))
        while self.obj.inv:
            self.obj.inv[0].Destroy()

    def test_Move(self):
        self.assertRaises(TypeError, self.obj.Move)
        self.assertRaises(TypeError, self.obj.Move, 1, 2)
        self.assertRaises(TypeError, self.obj.Move, x=1)

        m = Atrinik.CreateMap(5, 5, "test-atrinik-object-move")
        m.Insert(activator, 0, 0)
        self.assertEqual(activator.Move(Atrinik.EAST), Atrinik.EAST)
        self.assertEqual(activator.x, 1)
        self.assertEqual(activator.y, 0)
        self.assertEqual(activator.Move(Atrinik.NORTH), 0)
        self.assertEqual(activator.x, 1)
        self.assertEqual(activator.y, 0)
        m.CreateObject("door1_locked", 2, 0)
        self.assertEqual(activator.Move(Atrinik.EAST), -1)
        self.assertEqual(activator.x, 1)
        self.assertEqual(activator.y, 0)
        self.assertEqual(activator.Move(Atrinik.EAST), Atrinik.EAST)
        self.assertEqual(activator.x, 2)
        self.assertEqual(activator.y, 0)

    def test_ConnectionTrigger(self):
        self.assertRaises(TypeError, self.obj.ConnectionTrigger, 1, "2")
        self.assertRaises(TypeError, self.obj.ConnectionTrigger, x=1)

        m = Atrinik.CreateMap(5, 5, "test-atrinik-object-move")
        gate = m.CreateObject("gate_closed", 0, 0)
        gate.connected = 101
        switch = m.CreateObject("switch_wall", 2, 2)
        switch.connected = 101
        self.assertAlmostEqual(gate.speed, 0.0)
        self.assertEqual(gate.value, 0)
        self.assertEqual(gate.state, gate.arch.clone.state)
        self.assertEqual(gate.wc, gate.arch.clone.wc)
        self.assertEqual(switch.value, 0)
        self.assertEqual(switch.state, 0)
        switch.ConnectionTrigger()
        self.assertEqual(switch.value, 1)
        self.assertEqual(switch.state, 1)
        self.assertNotAlmostEqual(gate.speed, 0.0)
        self.assertEqual(gate.value, 1)
        simulate_server(count=20, wait=False)
        self.assertEqual(gate.state, 0)
        self.assertEqual(gate.wc, 0)
        switch.ConnectionTrigger(False)
        simulate_server(count=20, wait=False)
        self.assertAlmostEqual(gate.speed, 0.0)
        self.assertEqual(gate.value, 0)
        self.assertEqual(gate.state, gate.arch.clone.state)
        self.assertEqual(gate.wc, gate.arch.clone.wc)
        self.assertEqual(switch.value, 0)
        self.assertEqual(switch.state, 0)

        gate = m.CreateObject("gate_closed", 1, 1)
        gate.connected = 102
        button = m.CreateObject("button", 3, 3)
        button.connected = 102
        button.weight = 10000
        self.assertAlmostEqual(gate.speed, 0.0)
        self.assertEqual(gate.value, 0)
        self.assertEqual(gate.state, gate.arch.clone.state)
        self.assertEqual(gate.wc, gate.arch.clone.wc)
        self.assertEqual(button.value, 0)
        self.assertEqual(button.state, 0)
        button.ConnectionTrigger(button=True)
        self.assertAlmostEqual(gate.speed, 0.0)
        self.assertEqual(gate.value, 0)
        self.assertEqual(gate.state, gate.arch.clone.state)
        self.assertEqual(gate.wc, gate.arch.clone.wc)
        self.assertEqual(button.value, 0)
        self.assertEqual(button.state, 0)
        torch = m.CreateObject("torch", button.x, button.y)
        self.assertAlmostEqual(gate.speed, 0.0)
        self.assertEqual(gate.value, 0)
        self.assertEqual(gate.state, gate.arch.clone.state)
        self.assertEqual(gate.wc, gate.arch.clone.wc)
        self.assertEqual(button.value, 0)
        self.assertEqual(button.state, 0)
        torch.weight = button.weight
        button.ConnectionTrigger(button=True)
        self.assertEqual(button.value, 1)
        self.assertNotAlmostEqual(gate.speed, 0.0)
        self.assertEqual(gate.value, 1)
        simulate_server(count=20, wait=False)
        self.assertAlmostEqual(gate.speed, 0.0)
        self.assertEqual(gate.state, 0)
        self.assertEqual(gate.wc, 0)
        button.ConnectionTrigger(push=False, button=True)
        self.assertEqual(button.value, 1)
        self.assertAlmostEqual(gate.speed, 0.0)
        self.assertEqual(gate.value, 1)
        self.assertEqual(gate.state, 0)
        self.assertEqual(gate.wc, 0)
        torch.Destroy()
        self.assertEqual(button.value, 0)
        self.assertNotAlmostEqual(gate.speed, 0.0)
        self.assertEqual(gate.value, 0)
        self.assertEqual(gate.state, 0)
        simulate_server(count=20, wait=False)
        self.assertAlmostEqual(gate.speed, 0.0)
        self.assertEqual(gate.value, 0)
        self.assertEqual(gate.state, gate.arch.clone.state)
        self.assertEqual(gate.wc, gate.arch.clone.wc)
        self.assertEqual(button.value, 0)
        self.assertEqual(button.state, 0)

    def test_Artificate(self):
        self.assertRaises(TypeError, self.obj.Artificate)
        self.assertRaises(TypeError, self.obj.Artificate, 1, 2)
        self.assertRaises(TypeError, self.obj.Artificate, x=1)

        self.assertRaises(Atrinik.AtrinikError, self.obj.Artificate, "xxx")
        self.assertRaises(Atrinik.AtrinikError, self.obj.Artificate,
                          "cloak_lesser_fire")
        self.assertRaises(Atrinik.AtrinikError, self.obj.Artificate,
                          "beer_charob")

        self.obj.Artificate("weapon_less_fire")
        self.assertEqual(self.obj.artifact, "weapon_less_fire")
        self.assertGreater(self.obj.Attack(Atrinik.ATNR_FIRE), 0)
        self.assertTrue(self.obj.f_is_magical)
        self.assertGreater(self.obj.value, self.obj.arch.clone.value)
        self.assertRaises(Atrinik.AtrinikError, self.obj.Artificate,
                          "weapon_less_fire")
        self.assertRaises(Atrinik.AtrinikError, self.obj.Artificate,
                          "xxx")

    def test_Load(self):
        self.assertRaises(TypeError, self.obj.Load)
        self.assertRaises(TypeError, self.obj.Load, 1, 2)
        self.assertRaises(TypeError, self.obj.Load, x=1)

        self.assertFalse(self.obj.f_identified)
        self.obj.Load("identified 1\n")
        self.assertTrue(self.obj.f_identified)
        self.assertFalse(self.obj.f_is_magical)
        self.assertNotEqual(self.obj.value, 50000000)
        self.assertNotEqual(self.obj.weight, 1)
        self.obj.Load("is_magical 1\nvalue 50000000\nweight 1\n")
        self.assertTrue(self.obj.f_is_magical)
        self.assertEqual(self.obj.value, 50000000)
        self.assertEqual(self.obj.weight, 1)

    def test_GetPacket(self):
        self.assertRaises(TypeError, self.obj.GetPacket)
        self.assertRaises(TypeError, self.obj.GetPacket, 1, 2)
        self.assertRaises(TypeError, self.obj.GetPacket, x=1)

        self.obj.f_identified = True
        self.obj.title = "of Greatness"

        fmt, data = self.obj.GetPacket(activator.Controller(), Atrinik.UPD_NAME)
        self.assertEqual(fmt, "Hx")
        self.assertEqual(data[0], Atrinik.UPD_NAME)
        self.assertIn(self.obj.GetName().encode(), data[1])

        self.obj.glow = "ff0000"

        fmt, data = self.obj.GetPacket(activator.Controller(),
                                       Atrinik.UPD_NAME | Atrinik.UPD_GLOW)
        self.assertEqual(fmt, "Hx")
        self.assertEqual(data[0], Atrinik.UPD_NAME | Atrinik.UPD_GLOW)
        self.assertIn(self.obj.GetName().encode(), data[1])
        self.assertIn(self.obj.glow.encode(), data[1])




activator = Atrinik.WhoIsActivator()
me = Atrinik.WhoAmI()
suites = [
    unittest.TestLoader().loadTestsFromTestCase(ObjectTestCase)
]
