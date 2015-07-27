import os
import unittest

import Atrinik
from tests import TestSuite


class MapMethodsSuite(TestSuite):
    def setUp(self):
        super().setUp()
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


class MapFieldsSuite(TestSuite):
    def setUp(self):
        super().setUp()
        self.map = self.obj = Atrinik.CreateMap(24, 24, self.id())

    def field_compare(self, field, val):
        if field == "darkness":
            val = min(7, val)

        super().field_compare(field, val)

    def test_next(self):
        with self.assertRaises(TypeError):
            self.map.next = None

        self.assertIsNone(self.map.next)
        m = Atrinik.CreateMap(5, 5, "test-atrinik-map-next")
        self.assertEqual(self.map.next, m)

    def test_previous(self):
        with self.assertRaises(TypeError):
            self.map.previous = None

        self.assertIsNotNone(Atrinik.GetFirst("map").previous)
        m = Atrinik.CreateMap(5, 5, "test-atrinik-map-previous")
        self.assertEqual(m.previous, self.map)

    def test_name(self):
        with self.assertRaises(TypeError):
            self.map.name = 10

        self.map.name = "hello world"
        self.field_compare("name", "hello world")

    def test_msg(self):
        with self.assertRaises(TypeError):
            self.map.msg = 10

        self.map.msg = "hello world"
        self.field_compare("msg", "hello world")

        self.map.msg = "this\n\is\na\nmulti\nline\nmessage"
        self.field_compare("msg", "this\n\is\na\nmulti\nline\nmessage")

    def test_reset_timeout(self):
        self.field_test_int("reset_timeout", 32, True)

    def test_timeout(self):
        self.field_test_int("timeout", 32)

    def test_difficulty(self):
        self.field_test_int("difficulty", 16, True)

    def test_height(self):
        with self.assertRaises(TypeError):
            self.map.height = 10
        with self.assertRaises(TypeError):
            self.map.height = "xxx"

        self.assertEqual(self.map.height, 24)

    def test_width(self):
        with self.assertRaises(TypeError):
            self.map.width = 10
        with self.assertRaises(TypeError):
            self.map.width = "xxx"

        self.assertEqual(self.map.width, 24)

    def test_darkness(self):
        self.field_test_int("darkness", 8, True)

    def test_path(self):
        with self.assertRaises(TypeError):
            self.map.path = "xxx"
        with self.assertRaises(TypeError):
            self.map.path = 10

        self.assertEqual(self.map.path, self.map.GetPath())

    def test_enter_x(self):
        self.field_test_int("enter_x", 8, True)

    def test_enter_y(self):
        self.field_test_int("enter_y", 8, True)

    def test_region(self):
        with self.assertRaises(TypeError):
            self.map.region = "xxx"
        with self.assertRaises(TypeError):
            self.map.region = 10
        with self.assertRaises(TypeError):
            self.map.region = Atrinik.GetFirst("region")

        self.assertIsNone(self.map.region)
        m = Atrinik.ReadyMap("/shattered_islands/world_1_67")
        self.assertEqual(m.region.name, "brynknot")

    def test_bg_music(self):
        with self.assertRaises(TypeError):
            self.map.bg_music = 10

        self.map.bg_music = "music.ogg"
        self.field_compare("bg_music", "music.ogg")

    def test_weather(self):
        with self.assertRaises(TypeError):
            self.map.weather = 10

        self.map.weather = "snow"
        self.field_compare("weather", "snow")


class MapFlagsSuite(TestSuite):
    def setUp(self):
        super().setUp()
        self.map = self.obj = Atrinik.CreateMap(24, 24, self.id())

    def test_f_outdoor(self):
        self.flag_test("f_outdoor")

    def test_f_unique(self):
        self.flag_test("f_unique")

    def test_f_fixed_rtime(self):
        self.flag_test("f_fixed_rtime")

    def test_f_nomagic(self):
        self.flag_test("f_nomagic")

    def test_f_height_diff(self):
        self.flag_test("f_height_diff")

    def test_f_noharm(self):
        self.flag_test("f_noharm")

    def test_f_nosummon(self):
        self.flag_test("f_nosummon")

    def test_f_fixed_login(self):
        self.flag_test("f_fixed_login")

    def test_f_player_no_save(self):
        self.flag_test("f_player_no_save")

    def test_f_pvp(self):
        self.flag_test("f_pvp")

    def test_f_no_save(self):
        self.flag_test("f_no_save")


activator = Atrinik.WhoIsActivator()
me = Atrinik.WhoAmI()
suites = [
    unittest.TestLoader().loadTestsFromTestCase(MapMethodsSuite),
    unittest.TestLoader().loadTestsFromTestCase(MapFieldsSuite),
    unittest.TestLoader().loadTestsFromTestCase(MapFlagsSuite),
]
