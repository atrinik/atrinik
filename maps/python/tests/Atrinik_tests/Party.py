import random
import unittest

import Atrinik
from tests import TestSuite


class PartyCommonSuite(TestSuite):
    def setUp(self):
        super().setUp()
        activator.Controller().ExecuteCommand("/party leave")
        activator.Controller().ExecuteCommand("/party form test")
        self.party = self.obj = activator.Controller().party

    def tearDown(self):
        super().tearDown()
        activator.Controller().ExecuteCommand("/party leave")


class PartyMethodsSuite(PartyCommonSuite):
    def test_AddMember(self):
        self.assertRaises(TypeError, self.party.AddMember)
        self.assertRaises(TypeError, self.party.AddMember, 1, 2)
        self.assertRaises(TypeError, self.party.AddMember, x=1)

        obj = Atrinik.CreateObject("sword")
        self.assertRaises(ValueError, self.party.AddMember, obj)
        obj.Destroy()

        self.assertRaises(Atrinik.AtrinikError, self.party.AddMember, activator)

        self.party.AddMember(me)
        self.assertEqual(me.Controller().party, self.party)

        self.party.RemoveMember(me)
        self.assertIsNone(me.Controller().party)

    def test_RemoveMember(self):
        self.assertRaises(TypeError, self.party.RemoveMember)
        self.assertRaises(TypeError, self.party.RemoveMember, 1, 2)
        self.assertRaises(TypeError, self.party.RemoveMember, x=1)

        obj = Atrinik.CreateObject("sword")
        self.assertRaises(ValueError, self.party.RemoveMember, obj)
        obj.Destroy()

        self.assertRaises(Atrinik.AtrinikError, self.party.RemoveMember, me)

        self.party.RemoveMember(activator)
        self.assertIsNone(activator.Controller().party)

    def test_GetMembers(self):
        self.assertRaises(TypeError, self.party.GetMembers, 1, 2)
        self.assertRaises(TypeError, self.party.GetMembers, x=1)

        self.assertEqual(self.party.GetMembers(), [activator])
        self.party.AddMember(me)
        self.assertEqual(self.party.GetMembers(), [me, activator])
        self.party.RemoveMember(me)
        self.assertEqual(self.party.GetMembers(), [activator])

    def test_SendMessage(self):
        self.assertRaises(TypeError, self.party.SendMessage)
        self.assertRaises(TypeError, self.party.SendMessage, 1, 2)
        self.assertRaises(TypeError, self.party.SendMessage, x=1)

        self.assertRaises(Atrinik.AtrinikError, self.party.SendMessage, "test",
                          Atrinik.PARTY_MESSAGE_CHAT)

        for obj in (activator, me):
            obj.Controller().s_packets.clear()

        msg = "Hello world: {}!".format(random.random())
        self.party.SendMessage(msg, Atrinik.PARTY_MESSAGE_CHAT, activator)
        packets = activator.Controller().s_packets
        self.assertIn(msg.encode(), packets[len(packets) - 1])
        self.assertFalse(me.Controller().s_packets)

        self.party.AddMember(me)

        for obj in (activator, me):
            obj.Controller().s_packets.clear()

        msg = "Hello world: {}!".format(random.random())
        self.party.SendMessage(msg, Atrinik.PARTY_MESSAGE_CHAT, activator)

        for obj in (activator, me):
            packets = obj.Controller().s_packets
            self.assertIn(msg.encode(), packets[len(packets) - 1])

        self.party.RemoveMember(me)

        for obj in (activator, me):
            obj.Controller().s_packets.clear()

        msg = "Hello world: {}!".format(random.random())
        self.party.SendMessage(msg, Atrinik.PARTY_MESSAGE_CHAT, activator)
        packets = activator.Controller().s_packets
        self.assertIn(msg.encode(), packets[len(packets) - 1])
        self.assertFalse(me.Controller().s_packets)


class PartyFieldsSuite(PartyCommonSuite):
    def test_name(self):
        with self.assertRaises(TypeError):
            self.party.name = None

        self.assertEqual(self.party.name, "test")

    def test_leader(self):
        self.assertEqual(self.party.leader, activator.name)
        self.party.leader = me.name
        self.assertEqual(self.party.leader, me.name)

    def test_password(self):
        with self.assertRaises(TypeError):
            self.party.password = None

        self.assertEqual(self.party.password, "")
        activator.Controller().ExecuteCommand("/party password secret")
        self.assertEqual(self.party.password, "secret")


activator = Atrinik.WhoIsActivator()
me = Atrinik.WhoAmI()
suites = [
    unittest.TestLoader().loadTestsFromTestCase(PartyMethodsSuite),
    unittest.TestLoader().loadTestsFromTestCase(PartyFieldsSuite),
]
