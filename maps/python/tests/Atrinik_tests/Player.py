import os
import random
import struct
import unittest

import Atrinik
from tests import simulate_server, TestSuite


class PlayerCommonSuite(TestSuite):
    def setUp(self):
        self.pl = self.obj = activator.Controller()


class PlayerMethodsSuite(PlayerCommonSuite):
    def test_GetEquipment(self):
        self.assertRaises(TypeError, self.pl.GetEquipment)
        self.assertRaises(TypeError, self.pl.GetEquipment, 1, 2)
        self.assertRaises(TypeError, self.pl.GetEquipment, x=1)

        self.assertRaises(ValueError, self.pl.GetEquipment, -1)
        self.assertRaises(ValueError, self.pl.GetEquipment, 500)

        self.assertIsNone(self.pl.GetEquipment(Atrinik.PLAYER_EQUIP_AMULET))
        amulet = self.pl.ob.CreateObject("amulet_generic")
        self.pl.ob.Apply(amulet)
        self.assertEqual(self.pl.GetEquipment(Atrinik.PLAYER_EQUIP_AMULET),
                         amulet)

    def test_CanCarry(self):
        self.assertRaises(TypeError, self.pl.CanCarry)
        self.assertRaises(TypeError, self.pl.CanCarry, 1, 2)
        self.assertRaises(TypeError, self.pl.CanCarry, x=1)

        self.assertTrue(self.pl.CanCarry(1))
        self.assertFalse(self.pl.CanCarry(1000000000))

        obj = Atrinik.CreateObject("sword")
        self.assertTrue(self.pl.CanCarry(obj))
        obj.weight = 1000000000
        self.assertFalse(self.pl.CanCarry(obj))
        obj.nrof = 5000
        obj.weight = 1
        self.assertTrue(self.pl.CanCarry(obj))
        obj.weight = 10000
        self.assertFalse(self.pl.CanCarry(obj))
        obj.Destroy()

    def test_AddExp(self):
        self.assertRaises(TypeError, self.pl.AddExp)
        self.assertRaises(TypeError, self.pl.AddExp, 1, "2")
        self.assertRaises(TypeError, self.pl.AddExp, x=1)
        self.assertRaises(TypeError, self.pl.AddExp, None, 100)

        self.assertRaises(ValueError, self.pl.AddExp, -1, 100)
        self.assertRaises(ValueError, self.pl.AddExp, 50000, 100)
        self.assertRaises(ValueError, self.pl.AddExp, "sorcery", 100)

        packets = self.pl.s_packets
        packets.clear()
        skill = self.pl.ob.FindObject(name="wizardry spells")
        self.assertIsNotNone(skill)
        skill.level = 1
        skill.exp = 0
        self.pl.AddExp("wizardry spells", 100)
        packet = b"".join(packets)
        self.assertIn("You got 100 exp in skill wizardry spells.".encode(),
                      packet)
        self.assertEqual(skill.exp, 100)
        packets.clear()
        skill.level = 1
        skill.exp = 0
        self.pl.AddExp("wizardry spells", 10000)
        packet = b"".join(packets)
        self.assertIn("You got 500 exp in skill wizardry spells.".encode(),
                      packet)
        self.assertEqual(skill.exp, 500)
        packets.clear()
        skill.level = 1
        skill.exp = 0
        self.pl.AddExp("wizardry spells", 10000, True)
        packet = b"".join(packets)
        self.assertIn("You got 10000 exp in skill wizardry spells.".encode(),
                      packet)
        self.assertEqual(skill.exp, 10000)
        packets.clear()
        skill.level = 1
        skill.exp = 0
        self.pl.AddExp("wizardry spells", 1, True, True)
        packet = b"".join(packets)
        self.assertIn("You are now level 2 in the skill wizardry "
                      "spells.".encode(), packet)
        self.assertEqual(skill.exp, 2000)
        self.assertEqual(skill.level, 2)
        packets.clear()
        skill.level = 1
        skill.exp = 0
        self.pl.AddExp("wizardry spells", 1, False, True)
        packet = b"".join(packets)
        self.assertIn("You got 500 exp in skill wizardry spells.".encode(),
                      packet)
        self.assertEqual(skill.exp, 500)
        self.assertEqual(skill.level, 1)

    def test_BankDeposit(self):
        self.assertRaises(TypeError, self.pl.BankDeposit)
        self.assertRaises(TypeError, self.pl.BankDeposit, 1, 2)
        self.assertRaises(TypeError, self.pl.BankDeposit, x=1)

        force = self.pl.ob.FindObject(archname="player_info",
                                      name="BANK_GENERAL")
        if force:
            force.Destroy()

        self.assertEqual(self.pl.BankDeposit("")[0],
                         Atrinik.BANK_SYNTAX_ERROR)
        self.assertEqual(self.pl.BankDeposit("50 mithril")[0],
                         Atrinik.BANK_DEPOSIT_MITHRIL)
        self.pl.ob.CreateObject("mithrilcoin")
        self.assertEqual(self.pl.BankDeposit("1 mithril"),
                         (Atrinik.BANK_SUCCESS, 10000000))
        self.assertEqual(self.pl.BankBalance(), 10000000)

    def test_BankWithdraw(self):
        self.assertRaises(TypeError, self.pl.BankWithdraw)
        self.assertRaises(TypeError, self.pl.BankWithdraw, 1, 2)
        self.assertRaises(TypeError, self.pl.BankWithdraw, x=1)

        force = self.pl.ob.FindObject(archname="player_info",
                                      name="BANK_GENERAL")
        if force:
            force.Destroy()

        self.assertEqual(self.pl.BankWithdraw("1 mithril")[0],
                         Atrinik.BANK_WITHDRAW_MISSING)
        force = self.pl.ob.CreateObject("player_info")
        force.name = "BANK_GENERAL"
        force.value = 10000000
        self.assertEqual(self.pl.BankWithdraw("")[0],
                         Atrinik.BANK_SYNTAX_ERROR)
        self.assertEqual(self.pl.BankBalance(), 10000000)
        self.assertEqual(self.pl.BankWithdraw("5 mithril")[0],
                         Atrinik.BANK_WITHDRAW_MISSING)
        self.assertEqual(self.pl.BankWithdraw("10000000 copper")[0],
                         Atrinik.BANK_WITHDRAW_HIGH)
        self.assertEqual(self.pl.BankWithdraw("1000000 copper")[0],
                         Atrinik.BANK_WITHDRAW_OVERWEIGHT)
        self.assertEqual(self.pl.BankWithdraw("24 gold"),
                         (Atrinik.BANK_SUCCESS, 240000))
        self.assertEqual(self.pl.BankBalance(), 10000000 - 240000)

    def test_BankBalance(self):
        self.assertRaises(TypeError, self.pl.BankBalance, 1, 2)
        self.assertRaises(TypeError, self.pl.BankBalance, x=1)

        force = self.pl.ob.FindObject(archname="player_info",
                                      name="BANK_GENERAL")
        if force:
            force.Destroy()
        self.assertEqual(self.pl.BankBalance(), 0)
        force = self.pl.ob.CreateObject("player_info")
        force.name = "BANK_GENERAL"
        force.value = 10000000
        val = force.value
        self.assertEqual(self.pl.BankBalance(), val)
        self.assertEqual(self.pl.BankWithdraw("all"),
                         (Atrinik.BANK_SUCCESS, val))
        self.assertEqual(force.value, 0)
        self.assertEqual(self.pl.BankBalance(), 0)

    def test_SwapApartments(self):
        self.assertRaises(TypeError, self.pl.SwapApartments)
        self.assertRaises(TypeError, self.pl.SwapApartments, 1, 2)
        self.assertRaises(TypeError, self.pl.SwapApartments, x=1)

        import Apartments
        info = Apartments.apartments_info
        apartments = info["strakewood_island"]["apartments"]

        m = Atrinik.ReadyMap("/emergency")
        path = m.GetPath(apartments["normal"]["path"], True, self.pl.ob.name)
        if os.path.exists(path):
            os.unlink(path)
        path = m.GetPath(apartments["cheap"]["path"], True, self.pl.ob.name)
        m = Atrinik.ReadyMap(path)
        x = apartments["cheap"]["x"]
        y = apartments["cheap"]["y"]
        m.Insert(self.pl.ob, x, y)
        sword = m.CreateObject("sword", x, y)
        sword.magic = 1
        sword.title = "of testing"
        sword.msg = "Hello world: {}!".format(random.random())
        saved = sword.Save()
        layer = sword.layer

        x = apartments["normal"]["x"]
        y = apartments["normal"]["y"]
        self.assertTrue(self.pl.SwapApartments(apartments["cheap"]["path"],
                                               apartments["normal"]["path"],
                                               x, y))
        self.assertNotEqual(self.pl.ob.map.path, m.path)
        path = m.GetPath(apartments["normal"]["path"], True, self.pl.ob.name)
        m = Atrinik.ReadyMap(path)
        sword2 = m.GetLayer(x, y, layer)[0]
        self.assertEqual(sword2.Save(), saved)

    def test_ExecuteCommand(self):
        self.assertRaises(TypeError, self.pl.ExecuteCommand)
        self.assertRaises(TypeError, self.pl.ExecuteCommand, 1, 2)
        self.assertRaises(TypeError, self.pl.ExecuteCommand, x=1)

        msg = "Hello world: {}!".format(random.random())
        packets = self.pl.s_packets
        packets.clear()
        self.pl.ExecuteCommand("/chat {}".format(msg))
        self.assertIn(msg.encode(), packets[-1])

    def test_FindMarkedObject(self):
        self.assertRaises(TypeError, self.pl.FindMarkedObject, 1, 2)
        self.assertRaises(TypeError, self.pl.FindMarkedObject, x=1)

        obj = self.pl.ob.CreateObject("sword")
        self.assertIsNone(self.pl.FindMarkedObject())

        # TODO: remove hard-coded command ID (15 = item mark command)
        data = struct.pack("!HBI", 5, 15, obj.count)
        self.pl.s_packet_recv_cmd += data
        simulate_server(count=30, wait=False)
        self.assertEqual(self.pl.FindMarkedObject(), obj)
        self.pl.s_packet_recv_cmd += data
        simulate_server(count=30, wait=False)
        self.assertIsNone(self.pl.FindMarkedObject())
        self.pl.s_packet_recv_cmd += data
        simulate_server(count=30, wait=False)
        self.assertEqual(self.pl.FindMarkedObject(), obj)
        obj.Destroy()
        self.assertIsNone(self.pl.FindMarkedObject())

    def test_Sound(self):
        self.assertRaises(TypeError, self.pl.Sound)
        self.assertRaises(TypeError, self.pl.Sound, 1, 2)
        self.assertRaises(TypeError, self.pl.Sound, x=1)

        packets = self.pl.s_packets
        packets.clear()
        self.pl.Sound("sound_effect.ogg")
        self.assertIn("sound_effect.ogg".encode(), packets[-1])

        packets.clear()
        self.pl.Sound("sound_effect.ogg", x=10, y=5, loop=5, volume=50)
        self.assertIn("sound_effect.ogg".encode(), packets[-1])

    def test_Examine(self):
        self.assertRaises(TypeError, self.pl.Examine)
        self.assertRaises(TypeError, self.pl.Examine, 1, 2)
        self.assertRaises(TypeError, self.pl.Examine, x=1)

        obj = self.pl.ob.CreateObject("sword")
        obj.f_identified = True

        packets = self.pl.s_packets
        packets.clear()
        self.assertIsNone(self.pl.Examine(obj))
        self.assertIn(obj.GetName(self.pl.ob).encode(), b"".join(packets))

        packets.clear()
        text = self.pl.Examine(obj, True)
        self.assertFalse(packets)
        self.assertIn(obj.GetName(self.pl.ob), text)

    def test_SendPacket(self):
        self.assertRaises(TypeError, self.pl.SendPacket)
        self.assertRaises(TypeError, self.pl.SendPacket, 1, 2)
        self.assertRaises(TypeError, self.pl.SendPacket, x=1)

        packets = self.pl.s_packets
        self.pl.SendPacket(1, "s", self.pl.ob.name)
        self.assertEqual(packets[-1], self.pl.ob.name.encode() + b"\x00")

        self.pl.SendPacket(1, "x", self.pl.ob.name.encode())
        self.assertEqual(packets[-1], self.pl.ob.name.encode())

        fmt = "bBhHiI"
        vals = (-10, 200, -1 << 15, (1 << 16) - 1, -1 << 31, (1 << 32) - 1,
                -1 << 63, (1 << 64) - 1)
        data = struct.pack("!" + fmt + "qQ", *vals)
        self.pl.SendPacket(1, fmt + "lL", *vals)
        self.assertEqual(packets[-1], data)

        for i, c in enumerate("bhil"):
            j = 8 << i
            self.assertRaises(OverflowError, self.pl.SendPacket, 1, c, -1 << j)
            self.assertRaises(OverflowError, self.pl.SendPacket, 1, c, 1 << j)
            self.assertRaises(TypeError, self.pl.SendPacket, 1, c, "x")
            c = c.upper()
            self.assertRaises(OverflowError, self.pl.SendPacket, 1, c, -1)
            self.assertRaises(OverflowError, self.pl.SendPacket, 1, c, 1 << j)
            self.assertRaises(TypeError, self.pl.SendPacket, 1, c, "x")

        self.assertRaises(ValueError, self.pl.SendPacket, 1, "k", 10)
        self.assertRaises(TypeError, self.pl.SendPacket, 1, "s", 10)
        self.assertRaises(TypeError, self.pl.SendPacket, 1, "x", 10)

    def test_DrawInfo(self):
        self.assertRaises(TypeError, self.pl.DrawInfo)
        self.assertRaises(TypeError, self.pl.DrawInfo, 1, 2)
        self.assertRaises(TypeError, self.pl.DrawInfo, x=1)

        packets = self.pl.s_packets
        packets.clear()
        msg = "Hello world: {}!".format(random.random())
        self.pl.DrawInfo(msg)
        self.assertIn(msg.encode(), packets[-1])

        for obj in (activator, me):
            obj.Controller().s_packets.clear()

        msg = "Hello world: {}!".format(random.random())
        self.pl.DrawInfo(msg, broadcast=True)

        for obj in (activator, me):
            self.assertIn(msg.encode(), obj.Controller().s_packets[-1])

    def test_FactionGetBounty(self):
        self.assertRaises(TypeError, self.pl.FactionGetBounty)
        self.assertRaises(TypeError, self.pl.FactionGetBounty, 1, 2)
        self.assertRaises(TypeError, self.pl.FactionGetBounty, x=1)

        self.assertRaises(Atrinik.AtrinikError, self.pl.FactionGetBounty,
                          "brynknotxzxzx")

        self.assertAlmostEqual(self.pl.FactionGetBounty("brynknot"), 0.0)
        self.pl.factions["brynknot"] = 500
        self.assertAlmostEqual(self.pl.FactionGetBounty("brynknot"), 0.0)
        self.pl.factions["brynknot"] = -500
        self.assertAlmostEqual(self.pl.FactionGetBounty("brynknot"), 500.0)
        self.pl.factions.clear()
        self.assertAlmostEqual(self.pl.FactionGetBounty("brynknot"), 0.0)

    def test_FactionClearBounty(self):
        self.assertRaises(TypeError, self.pl.FactionClearBounty)
        self.assertRaises(TypeError, self.pl.FactionClearBounty, 1, 2)
        self.assertRaises(TypeError, self.pl.FactionClearBounty, x=1)

        self.pl.factions["brynknot"] = 500
        self.pl.FactionClearBounty("brynknot")
        self.assertAlmostEqual(self.pl.factions["brynknot"], 500.0)
        self.pl.factions["brynknot"] = -500
        self.pl.FactionClearBounty("brynknot")
        self.assertAlmostEqual(self.pl.factions["brynknot"], 0.0)
        self.pl.factions.clear()
        self.pl.FactionClearBounty("brynknot")
        self.assertAlmostEqual(self.pl.FactionGetBounty("brynknot"), 0.0)

    def test_InsertCoins(self):
        self.assertRaises(TypeError, self.pl.InsertCoins)
        self.assertRaises(TypeError, self.pl.InsertCoins, 1, 2)
        self.assertRaises(TypeError, self.pl.InsertCoins, x=1)

        for obj in self.pl.ob.FindObjects(mode=Atrinik.INVENTORY_ALL,
                                          type=Atrinik.Type.MONEY):
            obj.Destroy()

        self.pl.InsertCoins(1)
        obj = self.pl.ob.inv[0]
        self.assertEqual(obj.arch.name, "coppercoin")
        self.assertEqual(obj.nrof, 1)

        self.pl.InsertCoins(50)
        obj = self.pl.ob.inv[0]
        self.assertEqual(obj.arch.name, "coppercoin")
        self.assertEqual(obj.nrof, 51)

        self.pl.InsertCoins(42345)
        obj1 = self.pl.ob.inv[0]
        obj2 = self.pl.ob.inv[1]
        obj3 = self.pl.ob.inv[2]
        self.assertEqual(obj1.arch.name, "silvercoin")
        self.assertEqual(obj1.nrof, 23)
        self.assertEqual(obj2.arch.name, "goldcoin")
        self.assertEqual(obj2.nrof, 4)
        self.assertEqual(obj3.arch.name, "coppercoin")
        self.assertEqual(obj3.nrof, 96)

    def test_Save(self):
        self.assertRaises(TypeError, self.pl.Save, 1, 2)
        self.assertRaises(TypeError, self.pl.Save, x=1)

        path = self.pl.ob.map.GetPath(unique=True, name=self.pl.ob.name)
        path = os.path.dirname(path)
        path = os.path.join(path, "player.dat")

        if os.path.exists(path):
            os.unlink(path)

        self.pl.Save()
        self.assertTrue(os.path.exists(path))
        self.assertTrue(os.path.isfile(path))

    def test_Address(self):
        self.assertRaises(TypeError, self.pl.Address, 1, 2)
        self.assertRaises(TypeError, self.pl.Address, x=1)

        self.assertEqual(self.pl.Address(), "127.0.0.1")
        self.assertEqual(self.pl.Address(True), "127.0.0.1 13327")


class PlayerFieldsSuite(PlayerCommonSuite):
    def test_next(self):
        with self.assertRaises(TypeError):
            self.pl.next = me

        pl = Atrinik.GetFirst("player")
        while pl.next:
            pl = pl.next

        self.assertIsNone(pl.next)
        self.assertEqual(activator.Controller().next, me.Controller())

    def test_prev(self):
        with self.assertRaises(TypeError):
            self.pl.prev = me

        pl = Atrinik.GetFirst("player")
        self.assertIsNone(pl.prev)
        self.assertEqual(me.Controller().prev, activator.Controller())

    def test_party(self):
        with self.assertRaises(TypeError):
            self.pl.party = None

        self.assertIsNone(self.pl.party)
        self.pl.ExecuteCommand("/party form test")
        self.assertIsNotNone(self.pl.party)
        self.assertEqual(self.pl.party, Atrinik.GetFirst("party"))
        self.pl.ExecuteCommand("/party leave")

    def test_savebed_map(self):
        orig = self.pl.savebed_map
        self.pl.savebed_map = ""
        self.assertEqual(self.pl.savebed_map, "")
        self.pl.savebed_map = "/test"
        self.assertEqual(self.pl.savebed_map, "/test")
        self.pl.savebed_map = orig

    def test_bed_x(self):
        self.field_test_int("bed_x", 16)

    def test_bed_y(self):
        self.field_test_int("bed_y", 16)

    def test_ob(self):
        with self.assertRaises(TypeError):
            self.pl.ob = None

        self.assertEqual(self.pl.ob, activator)
        self.assertEqual(self.pl.ob.Controller().ob, self.pl.ob)

    def test_quest_container(self):
        with self.assertRaises(TypeError):
            self.pl.quest_container = None

        self.assertIsNotNone(self.pl.quest_container)
        self.pl.ob.f_no_fix_player = True
        self.pl.quest_container.Destroy()
        self.pl.ob.f_no_fix_player = False
        self.assertIsNone(self.pl.quest_container)
        ob = self.pl.ob.CreateObject("quest_container")
        self.assertIsNotNone(self.pl.quest_container)
        self.assertEqual(self.pl.quest_container, ob)

    def test_target_object(self):
        m = Atrinik.CreateMap(5, 5, "test-atrinik-player-target-object")
        m.Insert(self.pl.ob, 0, 0)
        raas = m.CreateObject("raas", 1, 1)
        raas.ac = 0
        raas.Update()
        self.pl.target_object = raas
        self.pl.combat = True
        hp = raas.hp
        simulate_server(count=1, wait=False)
        self.assertLess(raas.hp, hp)

    def test_no_chat(self):
        packets = self.pl.s_packets
        packets.clear()
        msg = "Hello world: {}!".format(random.random())
        self.pl.ExecuteCommand("/chat {}".format(msg))
        self.assertIn(msg.encode(), packets[-1])

        self.pl.no_chat = True
        packets.clear()
        msg = "Hello world: {}!".format(random.random())
        self.pl.ExecuteCommand("/chat {}".format(msg))
        self.assertNotIn(msg.encode(), packets[-1])
        self.assertIn("You are no longer allowed to chat.".encode(),
                      packets[-1])

        self.pl.no_chat = False
        packets.clear()
        msg = "Hello world: {}!".format(random.random())
        self.pl.ExecuteCommand("/chat {}".format(msg))
        self.assertIn(msg.encode(), packets[-1])

    def test_tcl(self):
        self.flag_test("tcl")
        self.pl.tcl = False
        self.pl.cmd_permissions.append("tcl")
        self.pl.ExecuteCommand("/tcl")
        self.assertTrue(self.pl.tcl)
        self.pl.ExecuteCommand("/tcl")
        self.assertFalse(self.pl.tcl)
        self.pl.cmd_permissions.remove("tcl")

    def test_tgm(self):
        self.flag_test("tgm")
        self.pl.tgm = False
        self.pl.cmd_permissions.append("tgm")
        self.pl.ExecuteCommand("/tgm")
        self.assertTrue(self.pl.tgm)
        self.pl.ExecuteCommand("/tgm")
        self.assertFalse(self.pl.tgm)
        self.pl.cmd_permissions.remove("tgm")

    def test_tli(self):
        self.flag_test("tli")
        self.pl.tli = False
        self.pl.cmd_permissions.append("tli")
        self.pl.ExecuteCommand("/tli")
        self.assertTrue(self.pl.tli)
        self.pl.ExecuteCommand("/tli")
        self.assertFalse(self.pl.tli)
        self.pl.cmd_permissions.remove("tli")

    def test_tls(self):
        self.flag_test("tls")
        self.pl.tls = False
        self.pl.cmd_permissions.append("tls")
        self.pl.ExecuteCommand("/tls")
        self.assertTrue(self.pl.tls)
        self.pl.ExecuteCommand("/tls")
        self.assertFalse(self.pl.tls)
        self.pl.cmd_permissions.remove("tls")

    def test_tsi(self):
        self.flag_test("tsi")
        self.pl.tsi = False
        self.pl.cmd_permissions.append("tsi")
        self.pl.ExecuteCommand("/tsi")
        self.assertTrue(self.pl.tsi)
        self.pl.ExecuteCommand("/tsi")
        self.assertFalse(self.pl.tsi)
        self.pl.cmd_permissions.remove("tsi")

    def test_cmd_permissions(self):
        self.pl.cmd_permissions.clear()
        self.assertEqual(len(self.pl.cmd_permissions), 0)
        self.assertFalse(self.pl.cmd_permissions)
        self.pl.cmd_permissions.append("[OP]")
        self.pl.cmd_permissions.append("/tcl")
        self.pl.cmd_permissions.append("[DEV]")
        self.assertEqual(self.pl.cmd_permissions.items(), ["[OP]", "/tcl",
                                                           "[DEV]"])
        self.assertEqual(len(self.pl.cmd_permissions), 3)
        self.assertTrue(self.pl.cmd_permissions)
        self.pl.cmd_permissions.remove("/tcl")
        self.assertEqual(self.pl.cmd_permissions.items(), ["[OP]", None,
                                                           "[DEV]"])
        self.assertEqual(len(self.pl.cmd_permissions), 3)
        self.assertTrue(self.pl.cmd_permissions)
        self.pl.cmd_permissions.clear()
        self.assertEqual(len(self.pl.cmd_permissions), 0)
        self.assertFalse(self.pl.cmd_permissions)

    def test_factions(self):
        self.pl.factions.clear()
        self.pl.factions["brynknot"] = 200
        self.assertAlmostEqual(self.pl.factions["brynknot"], 200.0)
        self.pl.factions["brynknot"] -= 42.384
        self.assertAlmostEqual(self.pl.factions["brynknot"], 157.616)
        self.assertIn("brynknot", self.pl.factions)
        del self.pl.factions["brynknot"]
        with self.assertRaises(KeyError):
            # noinspection PyStatementEffect
            self.pl.factions["brynknot"]
        self.assertNotIn("brynknot", self.pl.factions)
        self.assertEqual(len(self.pl.factions), 0)
        self.assertFalse(self.pl.factions)

    def test_fame(self):
        self.field_test_int("fame", 64)

    def test_container(self):
        self.assertIsNone(self.pl.container)
        sack = self.pl.ob.CreateObject("sack")
        self.pl.ob.Apply(sack)
        self.assertTrue(sack.f_applied)
        self.assertIsNone(self.pl.container)
        self.pl.ob.Apply(sack)
        self.assertEqual(self.pl.container, sack)
        self.assertTrue(sack.f_applied)
        self.pl.ob.Apply(sack)
        self.assertIsNone(self.pl.container)
        self.assertFalse(sack.f_applied)
        sack.Destroy()

    def test_combat(self):
        self.flag_test("combat")

    def test_combat_force(self):
        self.flag_test("combat_force")

    def test_s_ext_title_flag(self):
        self.flag_test("s_ext_title_flag")

    def test_s_socket_version(self):
        with self.assertRaises(TypeError):
            self.pl.s_socket_version = 1

        self.assertGreater(self.pl.s_socket_version, 1051)

    def test_s_packets(self):
        packets = self.pl.s_packets
        packets.clear()
        self.assertEqual(len(packets), 0)
        self.assertFalse(packets)
        msg = "hello world".encode()
        packets.append(msg)
        with self.assertRaises(TypeError):
            packets.append("hello!")
        self.assertEqual(len(packets), 1)
        self.assertTrue(packets)
        self.assertEqual(packets[0], msg)
        self.assertEqual(packets[-1], msg)
        msg2 = "hello pluto!".encode()
        packets.append(msg2)
        self.assertEqual(len(packets), 2)
        self.assertTrue(packets)
        self.assertEqual(packets[0], msg)
        self.assertEqual(packets[-2], msg)
        self.assertEqual(packets[1], msg2)
        self.assertEqual(packets[-1], msg2)


activator = Atrinik.WhoIsActivator()
me = Atrinik.WhoAmI()
suites = [
    unittest.TestLoader().loadTestsFromTestCase(PlayerMethodsSuite),
    unittest.TestLoader().loadTestsFromTestCase(PlayerFieldsSuite),
]
