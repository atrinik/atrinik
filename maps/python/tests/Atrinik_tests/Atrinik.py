import unittest
import time

import Atrinik


class AtrinikTestCase(unittest.TestCase):
    def test_LoadObject(self):
        self.assertRaises(TypeError, Atrinik.LoadObject)
        self.assertRaises(TypeError, Atrinik.LoadObject, 1, 2)
        self.assertRaises(TypeError, Atrinik.LoadObject, x=1)

        obj = Atrinik.LoadObject("arch sword\nname My Sword\nend\n")
        self.assertIsNotNone(obj)
        self.assertIsInstance(obj, Atrinik.Object.Object)
        self.assertEqual(obj.arch.name, "sword")
        self.assertEqual(obj.name, "My Sword")
        obj.Destroy()

    def test_ReadyMap(self):
        self.assertRaises(TypeError, Atrinik.ReadyMap)
        self.assertRaises(TypeError, Atrinik.ReadyMap, 1, 2)
        self.assertRaises(TypeError, Atrinik.ReadyMap, x=1)

        m = Atrinik.ReadyMap("/emergency")
        self.assertIsNotNone(m)
        self.assertIsInstance(m, Atrinik.Map.Map)
        self.assertEqual(m.path, "/emergency")
        self.assertEqual(m.f_unique, False)

        m = Atrinik.ReadyMap("/emergency", True)
        self.assertIsNotNone(m)
        self.assertIsInstance(m, Atrinik.Map.Map)
        self.assertEqual(m.path, "/emergency")
        self.assertEqual(m.f_unique, False)

        unique_path = m.GetPath(unique=True, name=activator.name)

        m = Atrinik.ReadyMap(unique_path, True)
        self.assertIsNotNone(m)
        self.assertIsInstance(m, Atrinik.Map.Map)
        self.assertEqual(m.path, unique_path)
        self.assertEqual(m.f_unique, True)

        m = Atrinik.ReadyMap(unique_path)
        self.assertIsNotNone(m)
        self.assertIsInstance(m, Atrinik.Map.Map)
        self.assertEqual(m.path, unique_path)
        self.assertEqual(m.f_unique, True)

    def test_FindPlayer(self):
        self.assertRaises(TypeError, Atrinik.FindPlayer)
        self.assertRaises(TypeError, Atrinik.FindPlayer, 1, 2)
        self.assertRaises(TypeError, Atrinik.FindPlayer, x=1)

        pl = Atrinik.FindPlayer("Tester")
        self.assertIsNotNone(pl)
        self.assertIsInstance(pl, Atrinik.Object.Object)
        self.assertEqual(pl, activator)
        self.assertEqual(pl.name, activator.name)

        pl = Atrinik.FindPlayer("tester")
        self.assertIsNotNone(pl)
        self.assertIsInstance(pl, Atrinik.Object.Object)
        self.assertEqual(pl, activator)
        self.assertEqual(pl.name, activator.name)

        pl = Atrinik.FindPlayer("Tester Testington")
        self.assertIsNotNone(pl)
        self.assertIsInstance(pl, Atrinik.Object.Object)
        self.assertEqual(pl, me)
        self.assertEqual(pl.name, me.name)

        pl = Atrinik.FindPlayer("tester testington")
        self.assertIsNotNone(pl)
        self.assertIsInstance(pl, Atrinik.Object.Object)
        self.assertEqual(pl, me)
        self.assertEqual(pl.name, me.name)

        pl = Atrinik.FindPlayer(" tester ")
        self.assertIsNone(pl)

        pl = Atrinik.FindPlayer(" tester testington ")
        self.assertIsNone(pl)

        pl = Atrinik.FindPlayer("abc")
        self.assertIsNone(pl)

    def test_PlayerExists(self):
        self.assertRaises(TypeError, Atrinik.PlayerExists)
        self.assertRaises(TypeError, Atrinik.PlayerExists, 1, 2)
        self.assertRaises(TypeError, Atrinik.PlayerExists, x=1)

        activator.Controller().Save()
        self.assertEqual(Atrinik.PlayerExists("Tester"), True)
        self.assertEqual(Atrinik.PlayerExists("tester"), True)

        me.Controller().Save()
        self.assertEqual(Atrinik.PlayerExists("Tester Testington"), True)
        self.assertEqual(Atrinik.PlayerExists("tester testington"), True)

        self.assertEqual(Atrinik.PlayerExists("a"), False)

    def test_WhoAmI(self):
        self.assertRaises(TypeError, Atrinik.WhoAmI, 1, 2)
        self.assertRaises(TypeError, Atrinik.WhoAmI, x=1)

        self.assertEqual(Atrinik.WhoAmI(), me)

    def test_WhoIsActivator(self):
        self.assertRaises(TypeError, Atrinik.WhoIsActivator, 1, 2)
        self.assertRaises(TypeError, Atrinik.WhoIsActivator, x=1)

        self.assertEqual(Atrinik.WhoIsActivator(), activator)

    def test_WhoIsOther(self):
        self.assertRaises(TypeError, Atrinik.WhoIsOther, 1, 2)
        self.assertRaises(TypeError, Atrinik.WhoIsOther, x=1)

        self.assertIsNone(Atrinik.WhoIsOther())

    def test_WhatIsEvent(self):
        self.assertRaises(TypeError, Atrinik.WhatIsEvent, 1, 2)
        self.assertRaises(TypeError, Atrinik.WhatIsEvent, x=1)

        self.assertIsNone(Atrinik.WhatIsEvent())

    def test_GetEventNumber(self):
        self.assertRaises(TypeError, Atrinik.GetEventNumber, 1, 2)
        self.assertRaises(TypeError, Atrinik.GetEventNumber, x=1)

        self.assertRaises(Atrinik.AtrinikError, Atrinik.GetEventNumber)

    def test_WhatIsMessage(self):
        self.assertRaises(TypeError, Atrinik.WhatIsMessage, 1, 2)
        self.assertRaises(TypeError, Atrinik.WhatIsMessage, x=1)

        self.assertEqual(Atrinik.WhatIsMessage(), "")

    def test_GetOptions(self):
        self.assertRaises(TypeError, Atrinik.GetOptions, 1, 2)
        self.assertRaises(TypeError, Atrinik.GetOptions, x=1)

        self.assertIsNone(Atrinik.GetOptions())

    def test_GetReturnValue(self):
        self.assertRaises(TypeError, Atrinik.GetOptions, 1, 2)
        self.assertRaises(TypeError, Atrinik.GetOptions, x=1)

        self.assertEqual(Atrinik.GetReturnValue(), 0)

    def test_SetReturnValue(self):
        self.assertRaises(TypeError, Atrinik.SetReturnValue)
        self.assertRaises(TypeError, Atrinik.SetReturnValue, 1, 2)
        self.assertRaises(TypeError, Atrinik.SetReturnValue, x=1)

        Atrinik.SetReturnValue(42)
        self.assertEqual(Atrinik.GetReturnValue(), 42)

        Atrinik.SetReturnValue(-666)
        self.assertEqual(Atrinik.GetReturnValue(), -666)

        self.assertRaises(OverflowError, Atrinik.SetReturnValue,
                          10000000000)

    def test_GetEventParameters(self):
        self.assertRaises(TypeError, Atrinik.GetEventParameters, 1, 2)
        self.assertRaises(TypeError, Atrinik.GetEventParameters, x=1)

        l = Atrinik.GetEventParameters()
        self.assertIsInstance(l, list)
        self.assertEqual(l, [0, 0, 0, 0])

    def test_RegisterCommand(self):
        self.assertRaises(TypeError, Atrinik.RegisterCommand)
        self.assertRaises(TypeError, Atrinik.RegisterCommand, 1, 2)
        self.assertRaises(TypeError, Atrinik.RegisterCommand, x=1)

        Atrinik.RegisterCommand("roll", 1.0)
        Atrinik.RegisterCommand("roll", 1.0, 1)

    def test_CreatePathname(self):
        self.assertRaises(TypeError, Atrinik.CreatePathname)
        self.assertRaises(TypeError, Atrinik.CreatePathname, 1, 2)
        self.assertRaises(TypeError, Atrinik.CreatePathname, x=1)

        self.assertEqual(Atrinik.CreatePathname("/emergency"),
                         Atrinik.GetSettings()["mapspath"] + "/emergency")

        self.assertEqual(Atrinik.CreatePathname("emergency"),
                         Atrinik.GetSettings()["mapspath"] + "/emergency")

        self.assertEqual(Atrinik.CreatePathname(""),
                         Atrinik.GetSettings()["mapspath"] + "/")

    def test_GetTime(self):
        self.assertRaises(TypeError, Atrinik.GetTime, 1, 2)
        self.assertRaises(TypeError, Atrinik.GetTime, x=1)

        d = Atrinik.GetTime()
        self.assertIsInstance(d, dict)
        self.assertIn("year", d)
        self.assertIsInstance(d["year"], int)
        self.assertIn("month", d)
        self.assertIsInstance(d["month"], int)
        self.assertIn("month_name", d)
        self.assertIsInstance(d["month_name"], str)
        self.assertIn("day", d)
        self.assertIsInstance(d["day"], int)
        self.assertIn("hour", d)
        self.assertIsInstance(d["hour"], int)
        self.assertIn("minute", d)
        self.assertIsInstance(d["minute"], int)
        self.assertIn("dayofweek", d)
        self.assertIsInstance(d["dayofweek"], int)
        self.assertIn("dayofweek_name", d)
        self.assertIsInstance(d["dayofweek_name"], str)
        self.assertIn("season", d)
        self.assertIsInstance(d["season"], int)
        self.assertIn("season_name", d)
        self.assertIsInstance(d["season_name"], str)
        self.assertIn("periodofday", d)
        self.assertIsInstance(d["periodofday"], int)
        self.assertIn("periodofday_name", d)
        self.assertIsInstance(d["periodofday_name"], str)

    def test_FindParty(self):
        self.assertRaises(TypeError, Atrinik.FindParty)
        self.assertRaises(TypeError, Atrinik.FindParty, 1, 2)
        self.assertRaises(TypeError, Atrinik.FindParty, x=1)

        self.assertIsNone(Atrinik.FindParty("xxx"))
        activator.Controller().ExecuteCommand("/party form xxx")
        party = Atrinik.FindParty("xxx")
        self.assertIsNotNone(party)
        self.assertIsInstance(party, Atrinik.Party.Party)
        self.assertEqual(party.name, "xxx")
        self.assertEqual(party.leader, activator.name)

    def test_Logger(self):
        self.assertRaises(TypeError, Atrinik.Logger)
        self.assertRaises(TypeError, Atrinik.Logger, 1, 2)
        self.assertRaises(TypeError, Atrinik.Logger, x=1)

        Atrinik.Logger("INFO", "Hello world!")

    def test_GetRangeVectorFromMapCoords(self):
        self.assertRaises(TypeError, Atrinik.GetRangeVectorFromMapCoords)
        self.assertRaises(TypeError, Atrinik.GetRangeVectorFromMapCoords, 1, 2)
        self.assertRaises(TypeError, Atrinik.GetRangeVectorFromMapCoords, x=1)

        m = Atrinik.ReadyMap("/emergency")
        t = Atrinik.GetRangeVectorFromMapCoords(m, 0, 0, m, 0, 0)
        self.assertIsInstance(t, tuple)
        self.assertEqual(t, (5, 0, 0, 0))

        m = Atrinik.CreateMap(50, 50, "test")
        t = Atrinik.GetRangeVectorFromMapCoords(m, 0, 0, m, 0, 0)
        self.assertIsInstance(t, tuple)
        self.assertEqual(t, (5, 0, 0, 0))
        t = Atrinik.GetRangeVectorFromMapCoords(m, 10, 10, m, 0, 0)
        self.assertIsInstance(t, tuple)
        self.assertEqual(t, (8, 20, -10, -10))
        t = Atrinik.GetRangeVectorFromMapCoords(m, 0, 0, m, 10, 10)
        self.assertIsInstance(t, tuple)
        self.assertEqual(t, (4, 20, 10, 10))

    def test_CostString(self):
        self.assertRaises(TypeError, Atrinik.CostString)
        self.assertRaises(TypeError, Atrinik.CostString, 1, 2)
        self.assertRaises(TypeError, Atrinik.CostString, x=1)

        self.assertEqual(Atrinik.CostString(0), "nothing")
        self.assertEqual(Atrinik.CostString(1), "1 copper coin")
        self.assertEqual(Atrinik.CostString(100), "1 silver coin")
        self.assertEqual(Atrinik.CostString(10000), "1 gold coin")
        self.assertEqual(Atrinik.CostString(1000000), "1 jade coin")
        self.assertEqual(Atrinik.CostString(10000000), "1 mithril coin")
        self.assertEqual(Atrinik.CostString(100000000), "1 amber coin")

        self.assertEqual(Atrinik.CostString(101),
                         "1 silver coin and 1 copper coin")
        self.assertEqual(Atrinik.CostString(2142),
                         "21 silver coins and 42 copper coins")
        self.assertEqual(Atrinik.CostString(116485),
                         "11 gold coins, 64 silver coins and 85 copper coins")

    def test_CacheAdd(self):
        self.assertRaises(TypeError, Atrinik.CacheAdd)
        self.assertRaises(TypeError, Atrinik.CacheAdd, 1, 2)
        self.assertRaises(TypeError, Atrinik.CacheAdd, x=1)

        self.assertEqual(Atrinik.CacheAdd("xxx", 0), True)
        Atrinik.CacheRemove("xxx")

        self.assertEqual(Atrinik.CacheAdd("xxx", 0), True)
        self.assertEqual(Atrinik.CacheAdd("xxx2", 0), True)
        self.assertEqual(Atrinik.CacheAdd("xxx2", 0), False)
        Atrinik.CacheRemove("xxx")
        Atrinik.CacheRemove("xxx2")

    def test_CacheGet(self):
        self.assertRaises(TypeError, Atrinik.CacheGet)
        self.assertRaises(TypeError, Atrinik.CacheGet, 1, 2)
        self.assertRaises(TypeError, Atrinik.CacheGet, x=1)

        self.assertRaises(ValueError, Atrinik.CacheGet, "xxx")

        obj = 0
        Atrinik.CacheAdd("xxx", obj)
        self.assertEqual(Atrinik.CacheGet("xxx"), 0)
        self.assertIs(Atrinik.CacheGet("xxx"), obj)
        Atrinik.CacheRemove("xxx")

        obj = activator
        Atrinik.CacheAdd("xxx", obj)
        self.assertEqual(Atrinik.CacheGet("xxx"), activator)
        self.assertEqual(Atrinik.CacheGet("xxx"), Atrinik.WhoIsActivator())
        self.assertIs(Atrinik.CacheGet("xxx"), activator)
        self.assertIsNot(Atrinik.CacheGet("xxx"), Atrinik.WhoIsActivator())
        Atrinik.CacheRemove("xxx")

        self.assertRaises(ValueError, Atrinik.CacheGet, "xxx")

    def test_CacheRemove(self):
        self.assertRaises(TypeError, Atrinik.CacheRemove)
        self.assertRaises(TypeError, Atrinik.CacheRemove, 1, 2)
        self.assertRaises(TypeError, Atrinik.CacheRemove, x=1)

        self.assertRaises(ValueError, Atrinik.CacheRemove, "xxx")

        Atrinik.CacheAdd("xxx", 0)
        self.assertEqual(Atrinik.CacheRemove("xxx"), True)

        self.assertRaises(ValueError, Atrinik.CacheRemove, "xxx")

    def test_GetFirst(self):
        self.assertRaises(TypeError, Atrinik.GetFirst)
        self.assertRaises(TypeError, Atrinik.GetFirst, 1, 2)
        self.assertRaises(TypeError, Atrinik.GetFirst, x=1)

        self.assertRaises(ValueError, Atrinik.GetFirst, "xxx")

        Atrinik.ReadyMap("/emergency")
        activator.Controller().ExecuteCommand("/party form yyy")

        self.assertIsInstance(Atrinik.GetFirst("player"), Atrinik.Player.Player)
        self.assertIsInstance(Atrinik.GetFirst("map"), Atrinik.Map.Map)
        self.assertIsInstance(Atrinik.GetFirst("party"), Atrinik.Party.Party)
        self.assertIsInstance(Atrinik.GetFirst("region"), Atrinik.Region.Region)

    def test_CreateMap(self):
        self.assertRaises(TypeError, Atrinik.CreateMap)
        self.assertRaises(TypeError, Atrinik.CreateMap, 1, 2)
        self.assertRaises(TypeError, Atrinik.CreateMap, x=1)

        m = Atrinik.CreateMap(5, 5, "test")
        self.assertEqual(m.width, 5)
        self.assertEqual(m.height, 5)
        self.assertEqual(m.path, "/python-maps/test")
        self.assertIsNone(m.name)

    def test_CreateObject(self):
        self.assertRaises(TypeError, Atrinik.CreateObject)
        self.assertRaises(TypeError, Atrinik.CreateObject, 1, 2)
        self.assertRaises(TypeError, Atrinik.CreateObject, x=1)

        obj = Atrinik.CreateObject("sword")
        self.assertIsNotNone(obj)
        self.assertIsInstance(obj, Atrinik.Object.Object)
        self.assertEqual(obj.arch.name, "sword")
        self.assertEqual(obj.name, "sword")
        obj.Destroy()

        self.assertRaises(Atrinik.AtrinikError, Atrinik.CreateObject, "xxx")

    def test_GetTicks(self):
        self.assertRaises(TypeError, Atrinik.GetTicks, 1, 2)
        self.assertRaises(TypeError, Atrinik.GetTicks, x=1)

        ticks = Atrinik.GetTicks()
        self.assertGreaterEqual(ticks, 0)
        Atrinik.Process()
        self.assertGreater(Atrinik.GetTicks(), ticks)

    def test_GetArchetype(self):
        self.assertRaises(TypeError, Atrinik.GetArchetype)
        self.assertRaises(TypeError, Atrinik.GetArchetype, 1, 2)
        self.assertRaises(TypeError, Atrinik.GetArchetype, x=1)

        arch = Atrinik.GetArchetype("sword")
        self.assertIsNotNone(arch)
        self.assertIsInstance(arch, Atrinik.Archetype.Archetype)
        self.assertEqual(arch.name, "sword")
        self.assertEqual(arch.clone.name, "sword")

        self.assertRaises(Atrinik.AtrinikError, Atrinik.GetArchetype, "xxx")

    # noinspection PyUnresolvedReferences
    def test_print(self):
        Atrinik.print("hi")
        Atrinik.print("test")
        Atrinik.print("hello", "world!")

    def test_Eval(self):
        self.assertRaises(TypeError, Atrinik.Eval)
        self.assertRaises(TypeError, Atrinik.Eval, 1, 2)
        self.assertRaises(TypeError, Atrinik.Eval, x=1)

        obj = Atrinik.CreateObject("sword")
        Atrinik.Eval("obj.name = 'Eval\\\'d Sword'")
        self.assertEqual(obj.name, "sword")
        Atrinik.Process()
        self.assertEqual(obj.name, "Eval'd Sword")
        obj.Destroy()

        obj = Atrinik.CreateObject("sword")
        now = time.time()
        Atrinik.Eval("obj.name = 'Eval\\\'d Sword'", 0.1)
        self.assertEqual(obj.name, "sword")
        Atrinik.Process()
        self.assertEqual(obj.name, "sword")

        while time.time() - now < 0.2:
            Atrinik.Process()

        self.assertEqual(obj.name, "Eval'd Sword")
        obj.Destroy()

        obj = Atrinik.CreateObject("sword")
        Atrinik.Eval("""
try:
    obj.name = 'Eval\\\'d Sword'
except ReferenceError:
    pass
        """)
        self.assertEqual(obj.name, "sword")
        obj.Destroy()
        Atrinik.Process()

    def test_GetSettings(self):
        self.assertRaises(TypeError, Atrinik.GetSettings, 1, 2)
        self.assertRaises(TypeError, Atrinik.GetSettings, x=1)

        d = Atrinik.GetSettings()
        self.assertIsNotNone(d)
        self.assertIsInstance(d, dict)
        self.assertIn("mapspath", d)

    def test_Process(self):
        self.assertRaises(TypeError, Atrinik.Process, 1, 2)
        self.assertRaises(TypeError, Atrinik.Process, x=1)

        ticks = Atrinik.GetTicks()
        Atrinik.Process()
        self.assertEqual(Atrinik.GetTicks() - ticks, 1)

        ticks = Atrinik.GetTicks()

        for _ in range(10):
            Atrinik.Process()

        self.assertEqual(Atrinik.GetTicks() - ticks, 10)


activator = Atrinik.WhoIsActivator()
me = Atrinik.WhoAmI()
suites = [
    unittest.TestLoader().loadTestsFromTestCase(AtrinikTestCase)
]
