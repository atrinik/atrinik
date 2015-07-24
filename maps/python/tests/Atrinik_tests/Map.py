import os
import unittest

import Atrinik
from tests import simulate_server


class MapMethodsSuite(unittest.TestCase):
    maxDiff = None

    def setUp(self):
        simulate_server(count=1, wait=False)
        self.map = Atrinik.CreateMap(24, 24, self.id())

    def test_Objects(self):
        self.assertRaises(TypeError, self.map.Objects)
        self.assertRaises(TypeError, self.map.Objects, 1, "2")
        self.assertRaises(TypeError, self.map.Objects, x=1)

        self.assertFalse(self.map.Objects(0, 0))
        self.assertEqual(len(self.map.Objects(0, 0)), 0)
        sword = self.map.CreateObject("sword", 0, 0)
        self.assertTrue(self.map.Objects(0, 0))
        self.assertEqual(len(self.map.Objects(0, 0)), 1)
        self.assertEqual(self.map.Objects(0, 0)[0], sword)
        self.map.Insert(activator, 0, 0)
        self.assertTrue(self.map.Objects(0, 0))
        self.assertEqual(len(self.map.Objects(0, 0)), 2)
        self.assertEqual(self.map.Objects(0, 0)[0], activator)
        self.assertEqual(self.map.Objects(0, 0)[1], sword)

    def test_ObjectsReversed(self):
        self.assertRaises(TypeError, self.map.ObjectsReversed)
        self.assertRaises(TypeError, self.map.ObjectsReversed, 1, "2")
        self.assertRaises(TypeError, self.map.ObjectsReversed, x=1)

        self.assertFalse(self.map.ObjectsReversed(0, 0))
        self.assertEqual(len(self.map.ObjectsReversed(0, 0)), 0)
        sword = self.map.CreateObject("sword", 0, 0)
        self.assertTrue(self.map.ObjectsReversed(0, 0))
        self.assertEqual(len(self.map.ObjectsReversed(0, 0)), 1)
        self.assertEqual(self.map.ObjectsReversed(0, 0)[0], sword)
        self.map.Insert(activator, 0, 0)
        self.assertTrue(self.map.ObjectsReversed(0, 0))
        self.assertEqual(len(self.map.ObjectsReversed(0, 0)), 2)
        self.assertEqual(self.map.ObjectsReversed(0, 0)[0], sword)
        self.assertEqual(self.map.ObjectsReversed(0, 0)[1], activator)

    def test_GetLayer(self):
        self.assertRaises(TypeError, self.map.GetLayer)
        self.assertRaises(TypeError, self.map.GetLayer, 1, 2)
        self.assertRaises(TypeError, self.map.GetLayer, x=1)

        self.assertRaises(Atrinik.AtrinikError, self.map.GetLayer, 50, 50, 1)
        self.assertRaises(ValueError, self.map.GetLayer, 0, 0, 50)

        self.assertEqual(self.map.GetLayer(0, 0, 0), [])
        l = []
        for _ in range(5):
            l.append(self.map.CreateObject("sword", 0, 0))
        l.reverse()
        self.assertEqual(self.map.GetLayer(0, 0, 0), [])
        self.assertEqual(self.map.GetLayer(0, 0, l[0].layer), l)
        sword = self.map.CreateObject("sword", 0, 0)
        sword.sub_layer = 3
        self.assertEqual(self.map.GetLayer(0, 0, l[0].layer, l[0].sub_layer), l)
        l.append(sword)
        self.assertEqual(self.map.GetLayer(0, 0, l[0].layer), l)
        self.map.Insert(activator, 0, 0)
        self.assertEqual(self.map.GetLayer(0, 0, l[0].layer), l)
        self.assertEqual(self.map.GetLayer(0, 0, activator.layer), [activator])
        self.map.Insert(me, 0, 0)
        self.assertEqual(self.map.GetLayer(0, 0, l[0].layer), l)
        self.assertEqual(self.map.GetLayer(0, 0, me.layer), [me, activator])

    def test_GetMapFromCoord(self):
        self.assertRaises(TypeError, self.map.GetMapFromCoord)
        self.assertRaises(TypeError, self.map.GetMapFromCoord, 1, "2")
        self.assertRaises(TypeError, self.map.GetMapFromCoord, x=1)

        self.assertEqual(self.map.GetMapFromCoord(0, 0), (self.map, 0, 0))
        self.assertEqual(self.map.GetMapFromCoord(self.map.width, 0),
                         (None, self.map.width, 0))

        m1 = Atrinik.ReadyMap("/shattered_islands/world_1_50")
        m2 = Atrinik.ReadyMap("/shattered_islands/world_2_50")

        self.assertEqual(m1.GetMapFromCoord(m1.width, 4), (m2, 0, 4))

        t = m1.GetMapFromCoord(5, m1.height)
        m2 = Atrinik.ReadyMap("/shattered_islands/world_1_51")
        self.assertEqual(t, (m2, 5, 0))

    def test_PlaySound(self):
        self.assertRaises(TypeError, self.map.PlaySound)
        self.assertRaises(TypeError, self.map.PlaySound, 1, 2)
        self.assertRaises(TypeError, self.map.PlaySound, x=1)

        self.map.Insert(activator, 0, 0)
        packets = activator.Controller().s_packets
        packets.clear()
        self.map.PlaySound("arrow_hit.ogg", 0, 0)
        self.assertIn("arrow_hit.ogg".encode(), packets[len(packets) - 1])
        packets.clear()
        self.map.PlaySound("arrow_hit.ogg", 23, 23)
        self.assertFalse(packets)

    def test_DrawInfo(self):
        self.assertRaises(TypeError, self.map.DrawInfo)
        self.assertRaises(TypeError, self.map.DrawInfo, 1, 2)
        self.assertRaises(TypeError, self.map.DrawInfo, x=1)

        self.map.Insert(activator, 0, 0)
        packets = activator.Controller().s_packets
        packets.clear()
        self.map.DrawInfo(0, 0, "hello world!")
        self.assertIn("hello world!".encode(), packets[len(packets) - 1])
        self.assertIn(Atrinik.COLOR_BLUE.encode(), packets[len(packets) - 1])
        packets.clear()
        self.map.DrawInfo(23, 23, "hello world!")
        self.assertFalse(packets)

    def test_CreateObject(self):
        self.assertRaises(TypeError, self.map.CreateObject)
        self.assertRaises(TypeError, self.map.CreateObject, 1, 2)
        self.assertRaises(TypeError, self.map.CreateObject, x=1)

        self.assertRaises(Atrinik.AtrinikError, self.map.CreateObject, "xxx",
                          0, 0)
        sword = self.map.CreateObject("sword", 0, 1)
        self.assertEqual(sword.arch.name, "sword")
        self.assertEqual(sword.map, self.map)
        self.assertEqual(sword.x, 0)
        self.assertEqual(sword.y, 1)
        self.assertEqual(self.map.Objects(0, 1)[0], sword)

    def test_CountPlayers(self):
        self.assertRaises(TypeError, self.map.CountPlayers, 1, 2)
        self.assertRaises(TypeError, self.map.CountPlayers, x=1)

        self.assertEqual(self.map.CountPlayers(), 0)
        self.map.Insert(activator, 0, 0)
        self.assertEqual(self.map.CountPlayers(), 1)
        self.map.Insert(me, 0, 0)
        self.assertEqual(self.map.CountPlayers(), 2)

        m = Atrinik.CreateMap(5, 5, "test-atrinik-map-count-players")
        m.Insert(me, 0, 0)
        self.assertEqual(self.map.CountPlayers(), 1)
        self.assertEqual(m.CountPlayers(), 1)

    def test_GetPlayers(self):
        self.assertRaises(TypeError, self.map.GetPlayers, 1, 2)
        self.assertRaises(TypeError, self.map.GetPlayers, x=1)

        self.assertEqual(self.map.GetPlayers(), [])
        self.map.Insert(activator, 0, 0)
        self.assertEqual(self.map.GetPlayers(), [activator])
        self.map.Insert(me, 0, 0)
        self.assertEqual(self.map.GetPlayers(), [me, activator])

        m = Atrinik.CreateMap(5, 5, "test-atrinik-map-count-players")
        m.Insert(me, 0, 0)
        self.assertEqual(self.map.GetPlayers(), [activator])
        self.assertEqual(m.GetPlayers(), [me])

    def test_Insert(self):
        self.assertRaises(TypeError, self.map.Insert)
        self.assertRaises(TypeError, self.map.Insert, 1, 2)
        self.assertRaises(TypeError, self.map.Insert, x=1)

        sword = Atrinik.CreateObject("sword")
        self.map.Insert(sword, 0, 1)
        self.assertEqual(sword.map, self.map)
        self.assertEqual(sword.x, 0)
        self.assertEqual(sword.y, 1)
        self.assertEqual(self.map.Objects(0, 1)[0], sword)

        self.map.Insert(sword, 1, 2)
        self.assertEqual(sword.map, self.map)
        self.assertEqual(sword.x, 1)
        self.assertEqual(sword.y, 2)
        self.assertFalse(self.map.Objects(0, 1))
        self.assertEqual(self.map.Objects(1, 2)[0], sword)

    def test_Wall(self):
        self.assertRaises(TypeError, self.map.Wall)
        self.assertRaises(TypeError, self.map.Wall, 1, "2")
        self.assertRaises(TypeError, self.map.Wall, x=1)

        self.assertEqual(self.map.Wall(0, 0), 0)
        self.map.CreateObject("blocked", 0, 0)
        self.assertEqual(self.map.Wall(0, 0), Atrinik.P_NO_PASS)

    def test_Blocked(self):
        self.assertRaises(TypeError, self.map.Blocked)
        self.assertRaises(TypeError, self.map.Blocked, 1, 2)
        self.assertRaises(TypeError, self.map.Blocked, x=1)

        self.assertEqual(self.map.Blocked(activator, 0, 0,
                                          activator.terrain_flag) &
                         Atrinik.P_NO_PASS, 0)
        self.map.CreateObject("blocked", 0, 0)
        self.assertEqual(self.map.Blocked(activator, 0, 0,
                                          activator.terrain_flag) &
                         Atrinik.P_NO_PASS, Atrinik.P_NO_PASS)

    def test_FreeSpot(self):
        self.assertRaises(TypeError, self.map.FreeSpot)
        self.assertRaises(TypeError, self.map.FreeSpot, 1, 2)
        self.assertRaises(TypeError, self.map.FreeSpot, x=1)

        self.assertRaises(ValueError, self.map.FreeSpot, me, 0, 0, -1, 1)
        self.assertRaises(ValueError, self.map.FreeSpot, me, 0, 0, 1, -1)
        self.assertRaises(ValueError, self.map.FreeSpot, me, 0, 0, 0, 5000)

        self.assertEqual(self.map.FreeSpot(me, 500, 0, 0, Atrinik.SIZEOFFREE3),
                         -1)
        self.assertEqual(self.map.FreeSpot(me, 0, 0, 0, 1), 0)
        self.map.CreateObject("blocked", 0, 0)
        self.assertEqual(self.map.FreeSpot(me, 0, 0, 0, 1), -1)
        self.assertEqual(self.map.FreeSpot(me, 1, 0, 0, 1), 0)

    def test_GetDarkness(self):
        self.assertRaises(TypeError, self.map.GetDarkness)
        self.assertRaises(TypeError, self.map.GetDarkness, 1, "2")
        self.assertRaises(TypeError, self.map.GetDarkness, x=1)

        self.assertRaises(Atrinik.AtrinikError, self.map.GetDarkness, 100, 0)

        self.assertEqual(self.map.GetDarkness(0, 0), 0)
        self.map.darkness = 3
        self.assertEqual(self.map.GetDarkness(0, 0), 80)

    def test_GetPath(self):
        self.assertRaises(TypeError, self.map.GetPath, 1, 2)
        self.assertRaises(TypeError, self.map.GetPath, x=1)

        self.assertEqual(self.map.GetPath(), self.map.path)
        self.assertEqual(self.map.GetPath(unique=True), self.map.path)
        self.assertEqual(self.map.GetPath("test"), "/python-maps/test")
        path = self.map.GetPath(unique=True, name=me.name)
        self.assertIn(me.name.lower(), path)
        self.assertIn(self.map.path.replace("/", "$"), path)

    def test_LocateBeacon(self):
        self.assertRaises(TypeError, self.map.LocateBeacon, 1, 2)
        self.assertRaises(TypeError, self.map.LocateBeacon, x=1)

        m = Atrinik.ReadyMap("/python/tests/data/test_locate_beacon")
        obj = m.LocateBeacon("test_atrinik_map_locate_beacon")
        self.assertEqual(obj.map, m)
        path = m.GetPath(unique=True, name=me.name)
        m2 = Atrinik.ReadyMap(path)
        obj2 = m2.LocateBeacon("test_atrinik_map_locate_beacon")
        self.assertEqual(obj2.map, m2)
        self.assertNotEqual(obj, obj2)

    def test_Redraw(self):
        self.assertRaises(TypeError, self.map.Redraw, 1, "2")
        self.assertRaises(TypeError, self.map.Redraw, x=1)

        self.assertRaises(Atrinik.AtrinikError, self.map.Redraw, -1, 0)
        self.assertRaises(Atrinik.AtrinikError, self.map.Redraw, 100, 0)
        self.assertRaises(Atrinik.AtrinikError, self.map.Redraw, 0, -1)
        self.assertRaises(Atrinik.AtrinikError, self.map.Redraw, 0, 100)
        self.assertRaises(Atrinik.AtrinikError, self.map.Redraw, 0, 0, -2)
        self.assertRaises(Atrinik.AtrinikError, self.map.Redraw, 0, 0, 50)
        self.assertRaises(Atrinik.AtrinikError, self.map.Redraw, 0, 0, 1, -2)
        self.assertRaises(Atrinik.AtrinikError, self.map.Redraw, 0, 0, 1, 50)

        self.map.Insert(me, 0, 0)
        self.map.Insert(activator, 1, 0)
        self.map.Redraw(0, 0)
        self.map.Redraw(1, 0)
        # TODO: would be nice if we could check that redrawing was successful

    def test_Save(self):
        self.assertRaises(TypeError, self.map.Save, 1, 2)
        self.assertRaises(TypeError, self.map.Save, x=1)

        m = Atrinik.ReadyMap("/emergency")
        m = Atrinik.ReadyMap(m.GetPath(unique=True, name=me.name))
        m.f_no_save = False

        if os.path.exists(m.path):
            os.unlink(m.path)

        m.Save()
        self.assertTrue(os.path.exists(m.path))


activator = Atrinik.WhoIsActivator()
me = Atrinik.WhoAmI()
suites = [
    unittest.TestLoader().loadTestsFromTestCase(MapMethodsSuite),
]
