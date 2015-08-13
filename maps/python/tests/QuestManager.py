import unittest
from collections import OrderedDict

import Atrinik
from tests import TestSuite
from QuestManager import QuestManager


class QuestManagerSuite(TestSuite):
    maxDiff = None

    def setUp(self):
        super().setUp()
        quest_container = activator.FindObject(archname="quest_container")
        self.assertTrue(quest_container)

        while quest_container.inv:
            quest_container.inv[0].Destroy()

    def test_01(self):
        quest = {
            "parts": OrderedDict((("deliver", {
                "info": "",
                "item": {
                    "arch": "sword",
                    "name": "quest sword",
                },
                "uid": "deliver",
                "name": "Delivery",
            }),)),
            "name": "Test Quest",
            "uid": "test_quest",
        }
        qm = QuestManager(activator, quest)
        self.assertFalse(qm.started("deliver"))
        self.assertFalse(qm.finished("deliver"))
        self.assertFalse(qm.completed("deliver"))
        self.assertFalse(qm.completed())
        self.assertTrue(qm.need_start("deliver"))
        self.assertFalse(qm.need_finish("deliver"))
        self.assertFalse(qm.need_complete("deliver"))
        self.assertFalse(qm.need_complete_before_start("deliver"))
        self.assertRaises(AssertionError, qm.complete, "deliver")
        qm.start("deliver")
        self.assertTrue(qm.started("deliver"))
        self.assertFalse(qm.finished("deliver"))
        self.assertFalse(qm.completed("deliver"))
        self.assertFalse(qm.completed())
        self.assertFalse(qm.need_start("deliver"))
        self.assertTrue(qm.need_finish("deliver"))
        self.assertFalse(qm.need_complete("deliver"))
        self.assertFalse(qm.need_complete_before_start("deliver"))
        sword = activator.CreateObject("sword")
        self.assertTrue(qm.started("deliver"))
        self.assertFalse(qm.finished("deliver"))
        self.assertFalse(qm.completed("deliver"))
        self.assertFalse(qm.completed())
        self.assertFalse(qm.need_start("deliver"))
        self.assertTrue(qm.need_finish("deliver"))
        self.assertFalse(qm.need_complete("deliver"))
        self.assertFalse(qm.need_complete_before_start("deliver"))
        sword.name = "quest sword"
        self.assertTrue(qm.started("deliver"))
        self.assertTrue(qm.finished("deliver"))
        self.assertFalse(qm.completed("deliver"))
        self.assertFalse(qm.completed())
        self.assertFalse(qm.need_start("deliver"))
        self.assertFalse(qm.need_finish("deliver"))
        self.assertTrue(qm.need_complete("deliver"))
        self.assertFalse(qm.need_complete_before_start("deliver"))
        qm.complete("deliver")
        self.assertFalse(sword)
        self.assertTrue(qm.started("deliver"))
        self.assertFalse(qm.finished("deliver"))
        self.assertTrue(qm.completed("deliver"))
        self.assertTrue(qm.completed())
        self.assertFalse(qm.need_start("deliver"))
        self.assertFalse(qm.need_finish("deliver"))
        self.assertFalse(qm.need_complete("deliver"))
        self.assertFalse(qm.need_complete_before_start("deliver"))

    def test_02(self):
        quest = {
            "parts": OrderedDict((("deliver", {
                "info": "",
                "item": {
                    "arch": "torch",
                    "name": "quest torch",
                    "nrof": 50,
                },
                "uid": "deliver",
                "name": "Delivery",
            }),)),
            "name": "Test Quest",
            "uid": "test_quest",
        }
        qm = QuestManager(activator, quest)
        self.assertFalse(qm.started("deliver"))
        self.assertFalse(qm.finished("deliver"))
        self.assertFalse(qm.completed("deliver"))
        self.assertFalse(qm.completed())
        torch = activator.CreateObject("torch")
        torch.name = "quest torch"
        self.assertFalse(qm.started("deliver"))
        self.assertFalse(qm.finished("deliver"))
        self.assertFalse(qm.completed("deliver"))
        self.assertFalse(qm.completed())
        self.assertTrue(qm.need_start("deliver"))
        self.assertFalse(qm.need_finish("deliver"))
        self.assertFalse(qm.need_complete("deliver"))
        self.assertFalse(qm.need_complete_before_start("deliver"))
        torch.nrof = 50
        self.assertFalse(qm.started("deliver"))
        self.assertTrue(qm.finished("deliver"))
        self.assertFalse(qm.completed("deliver"))
        self.assertFalse(qm.completed())
        self.assertTrue(qm.need_start("deliver"))
        self.assertFalse(qm.need_finish("deliver"))
        self.assertFalse(qm.need_complete("deliver"))
        self.assertTrue(qm.need_complete_before_start("deliver"))
        qm.start("deliver")
        self.assertTrue(qm.started("deliver"))
        self.assertTrue(qm.finished("deliver"))
        self.assertFalse(qm.completed("deliver"))
        self.assertFalse(qm.completed())
        self.assertFalse(qm.need_start("deliver"))
        self.assertFalse(qm.need_finish("deliver"))
        self.assertTrue(qm.need_complete("deliver"))
        self.assertFalse(qm.need_complete_before_start("deliver"))
        qm.complete("deliver")
        self.assertFalse(torch)
        self.assertTrue(qm.started("deliver"))
        self.assertFalse(qm.finished("deliver"))
        self.assertTrue(qm.completed("deliver"))
        self.assertTrue(qm.completed())
        self.assertFalse(qm.need_start("deliver"))
        self.assertFalse(qm.need_finish("deliver"))
        self.assertFalse(qm.need_complete("deliver"))
        self.assertFalse(qm.need_complete_before_start("deliver"))

    def test_03(self):
        quest = {
            "parts": OrderedDict((("kill", {
                "info": "",
                "kill": {"nrof": 2},
                "uid": "kill",
                "name": "Killing",
            }),)),
            "name": "Test Quest",
            "uid": "test_quest",
        }
        qm = QuestManager(activator, quest)
        self.assertFalse(qm.started("kill"))
        self.assertFalse(qm.finished("kill"))
        self.assertFalse(qm.completed("kill"))
        self.assertFalse(qm.completed())
        self.assertTrue(qm.need_start("kill"))
        self.assertFalse(qm.need_finish("kill"))
        self.assertFalse(qm.need_complete("kill"))
        self.assertFalse(qm.need_complete_before_start("kill"))
        self.assertRaises(AssertionError, qm.complete, "kill")
        qm.start("kill")
        self.assertTrue(qm.started("kill"))
        self.assertFalse(qm.finished("kill"))
        self.assertFalse(qm.completed("kill"))
        self.assertFalse(qm.completed())
        self.assertFalse(qm.need_start("kill"))
        self.assertTrue(qm.need_finish("kill"))
        self.assertFalse(qm.need_complete("kill"))
        self.assertFalse(qm.need_complete_before_start("kill"))

        def create_raas():
            obj = activator.map.CreateObject("raas", activator.x, activator.y)
            quest_container = obj.CreateObject("quest_container")
            quest_container.name = "test_quest"
            quest_object = quest_container.CreateObject("quest_container")
            quest_object.name = "kill"
            quest_object.sub_type = Atrinik.QUEST_TYPE_KILL
            obj.Update()
            return obj

        raas = create_raas()
        activator.Hit(raas, -1)
        self.assertTrue(qm.started("kill"))
        self.assertFalse(qm.finished("kill"))
        self.assertFalse(qm.completed("kill"))
        self.assertFalse(qm.completed())
        self.assertFalse(qm.need_start("kill"))
        self.assertTrue(qm.need_finish("kill"))
        self.assertFalse(qm.need_complete("kill"))
        self.assertFalse(qm.need_complete_before_start("kill"))
        raas = create_raas()
        activator.Hit(raas, -1)
        self.assertTrue(qm.started("kill"))
        self.assertTrue(qm.finished("kill"))
        self.assertFalse(qm.completed("kill"))
        self.assertFalse(qm.completed())
        self.assertFalse(qm.need_start("kill"))
        self.assertFalse(qm.need_finish("kill"))
        self.assertTrue(qm.need_complete("kill"))
        self.assertFalse(qm.need_complete_before_start("kill"))
        qm.complete("kill")
        self.assertTrue(qm.started("kill"))
        self.assertTrue(qm.finished("kill"))
        self.assertTrue(qm.completed("kill"))
        self.assertTrue(qm.completed())
        self.assertFalse(qm.need_start("kill"))
        self.assertFalse(qm.need_finish("kill"))
        self.assertFalse(qm.need_complete("kill"))
        self.assertFalse(qm.need_complete_before_start("kill"))

    def test_04(self):
        quest = {
            "parts": OrderedDict((("special", {
                "info": "",
                "uid": "special",
                "name": "Special",
            }),)),
            "name": "Test Quest",
            "uid": "test_quest",
        }
        qm = QuestManager(activator, quest)
        self.assertFalse(qm.started("special"))
        self.assertTrue(qm.finished("special"))
        self.assertFalse(qm.completed("special"))
        self.assertFalse(qm.completed())
        self.assertTrue(qm.need_complete_before_start("special"))
        qm.start("special")
        self.assertTrue(qm.started("special"))
        self.assertTrue(qm.finished("special"))
        self.assertFalse(qm.completed("special"))
        self.assertFalse(qm.completed())
        self.assertFalse(qm.need_start("special"))
        self.assertFalse(qm.need_finish("special"))
        self.assertTrue(qm.need_complete("special"))
        self.assertFalse(qm.need_complete_before_start("special"))
        qm.complete("special")
        self.assertTrue(qm.started("special"))
        self.assertTrue(qm.finished("special"))
        self.assertTrue(qm.completed("special"))
        self.assertTrue(qm.completed())
        self.assertFalse(qm.need_start("special"))
        self.assertFalse(qm.need_finish("special"))
        self.assertFalse(qm.need_complete("special"))
        self.assertFalse(qm.need_complete_before_start("special"))

    def test_05(self):
        quest = {
            "parts": OrderedDict((("special", {
                "info": "",
                "uid": "special",
                "name": "Special",
                "parts": OrderedDict((("get_item", {
                    "info": "",
                    "uid": "get_item",
                    "name": "Get an item",
                    "item": {"arch": "sword", "name": "quest sword"},
                    "parts": OrderedDict((("kill", {
                        "info": "",
                        "uid": "kill",
                        "name": "Killing",
                        "kill": {"nrof": 1},
                    }),)),
                }),)),
            }),)),
            "name": "Test Quest",
            "uid": "test_quest",
        }
        qm = QuestManager(activator, quest)
        self.assertFalse(qm.started("special"))
        self.assertTrue(qm.finished("special"))
        self.assertFalse(qm.completed("special"))
        self.assertFalse(qm.completed())
        self.assertTrue(qm.need_complete_before_start("special"))
        qm.start("special")
        self.assertTrue(qm.started("special"))
        self.assertTrue(qm.finished("special"))
        self.assertFalse(qm.completed("special"))
        self.assertFalse(qm.completed())
        self.assertFalse(qm.need_start("special"))
        self.assertFalse(qm.need_finish("special"))
        self.assertTrue(qm.need_complete("special"))
        self.assertFalse(qm.need_complete_before_start("special"))
        
        self.assertFalse(qm.started(["special", "get_item"]))
        self.assertFalse(qm.finished(["special", "get_item"]))
        self.assertFalse(qm.completed(["special", "get_item"]))
        self.assertFalse(qm.completed())
        self.assertTrue(qm.need_start(["special", "get_item"]))
        self.assertFalse(qm.need_finish(["special", "get_item"]))
        self.assertFalse(qm.need_complete(["special", "get_item"]))
        self.assertFalse(qm.need_complete_before_start(["special", "get_item"]))
        self.assertFalse(qm.complete(["special", "get_item"]))
        qm.start(["special", "get_item"])
        self.assertTrue(qm.started(["special", "get_item"]))
        self.assertFalse(qm.finished(["special", "get_item"]))
        self.assertFalse(qm.completed(["special", "get_item"]))
        self.assertFalse(qm.completed())
        self.assertFalse(qm.need_start(["special", "get_item"]))
        self.assertTrue(qm.need_finish(["special", "get_item"]))
        self.assertFalse(qm.need_complete(["special", "get_item"]))
        self.assertFalse(qm.need_complete_before_start(["special", "get_item"]))

        self.assertFalse(qm.started(["special", "get_item", "kill"]))
        self.assertFalse(qm.finished(["special", "get_item", "kill"]))
        self.assertFalse(qm.completed(["special", "get_item", "kill"]))
        self.assertFalse(qm.completed())
        self.assertTrue(qm.need_start(["special", "get_item", "kill"]))
        self.assertFalse(qm.need_finish(["special", "get_item", "kill"]))
        self.assertFalse(qm.need_complete(["special", "get_item", "kill"]))
        self.assertFalse(qm.need_complete_before_start(["special", "get_item",
                                                        "kill"]))
        self.assertFalse(qm.complete(["special", "get_item", "kill"]))
        qm.start(["special", "get_item", "kill"])
        self.assertTrue(qm.started(["special", "get_item", "kill"]))
        self.assertFalse(qm.finished(["special", "get_item", "kill"]))
        self.assertFalse(qm.completed(["special", "get_item", "kill"]))
        self.assertFalse(qm.completed())
        self.assertFalse(qm.need_start(["special", "get_item", "kill"]))
        self.assertTrue(qm.need_finish(["special", "get_item", "kill"]))
        self.assertFalse(qm.need_complete(["special", "get_item", "kill"]))
        self.assertFalse(qm.need_complete_before_start(["special", "get_item",
                                                        "kill"]))

        raas = activator.map.CreateObject("raas", activator.x, activator.y)
        quest_container = raas.CreateObject("quest_container")
        quest_container.name = "test_quest"
        quest_object = quest_container.CreateObject("quest_container")
        quest_object.name = "kill"
        quest_object.sub_type = Atrinik.QUEST_TYPE_KILL
        raas.Update()
        activator.Hit(raas, -1)
        self.assertTrue(qm.started(["special", "get_item", "kill"]))
        self.assertTrue(qm.finished(["special", "get_item", "kill"]))
        self.assertFalse(qm.completed(["special", "get_item", "kill"]))
        self.assertFalse(qm.completed())
        self.assertFalse(qm.need_start(["special", "get_item", "kill"]))
        self.assertFalse(qm.need_finish(["special", "get_item", "kill"]))
        self.assertTrue(qm.need_complete(["special", "get_item", "kill"]))
        self.assertFalse(qm.need_complete_before_start(["special", "get_item",
                                                        "kill"]))
        qm.complete(["special", "get_item", "kill"])
        self.assertTrue(qm.started(["special", "get_item", "kill"]))
        self.assertTrue(qm.finished(["special", "get_item", "kill"]))
        self.assertTrue(qm.completed(["special", "get_item", "kill"]))
        self.assertFalse(qm.completed())
        self.assertFalse(qm.need_start(["special", "get_item", "kill"]))
        self.assertFalse(qm.need_finish(["special", "get_item", "kill"]))
        self.assertFalse(qm.need_complete(["special", "get_item", "kill"]))
        self.assertFalse(qm.need_complete_before_start(["special", "get_item",
                                                        "kill"]))
        
        sword = activator.CreateObject("sword")
        self.assertTrue(qm.started(["special", "get_item"]))
        self.assertFalse(qm.finished(["special", "get_item"]))
        self.assertFalse(qm.completed(["special", "get_item"]))
        self.assertFalse(qm.completed())
        self.assertFalse(qm.need_start(["special", "get_item"]))
        self.assertTrue(qm.need_finish(["special", "get_item"]))
        self.assertFalse(qm.need_complete(["special", "get_item"]))
        self.assertFalse(qm.need_complete_before_start(["special", "get_item"]))
        sword.name = "quest sword"
        self.assertTrue(qm.started(["special", "get_item"]))
        self.assertTrue(qm.finished(["special", "get_item"]))
        self.assertFalse(qm.completed(["special", "get_item"]))
        self.assertFalse(qm.completed())
        self.assertFalse(qm.need_start(["special", "get_item"]))
        self.assertFalse(qm.need_finish(["special", "get_item"]))
        self.assertTrue(qm.need_complete(["special", "get_item"]))
        self.assertFalse(qm.need_complete_before_start(["special", "get_item"]))
        qm.complete(["special", "get_item"])
        self.assertFalse(sword)
        self.assertTrue(qm.started(["special", "get_item"]))
        self.assertFalse(qm.finished(["special", "get_item"]))
        self.assertTrue(qm.completed(["special", "get_item"]))
        self.assertFalse(qm.completed())
        self.assertFalse(qm.need_start(["special", "get_item"]))
        self.assertFalse(qm.need_finish(["special", "get_item"]))
        self.assertFalse(qm.need_complete(["special", "get_item"]))
        self.assertFalse(qm.need_complete_before_start(["special", "get_item"]))
        
        qm.complete("special")
        self.assertTrue(qm.started("special"))
        self.assertTrue(qm.finished("special"))
        self.assertTrue(qm.completed("special"))
        self.assertTrue(qm.completed())
        self.assertFalse(qm.need_start("special"))
        self.assertFalse(qm.need_finish("special"))
        self.assertFalse(qm.need_complete("special"))
        self.assertFalse(qm.need_complete_before_start("special"))

    def test_06(self):
        quest = {
            "parts": OrderedDict((("special", {
                "info": "",
                "uid": "special",
                "name": "Special",
                "parts": OrderedDict((("special2", {
                    "info": "",
                    "uid": "special2",
                    "name": "Special 2",
                    "parts": OrderedDict((("kill", {
                        "info": "",
                        "uid": "kill",
                        "name": "Killing",
                        "kill": {"nrof": 1},
                    }),)),
                }),)),
            }),)),
            "name": "Test Quest",
            "uid": "test_quest",
        }
        qm = QuestManager(activator, quest)
        qm.start("special")
        self.assertTrue(qm.started("special"))
        self.assertFalse(qm.started(["special", "special2"]))
        qm.start(["special", "special2"])
        self.assertTrue(qm.started(["special", "special2"]))
        self.assertTrue(qm.need_complete("special"))
        qm.start(["special", "special2", "kill"])
        self.assertTrue(qm.started(["special", "special2", "kill"]))
        qm.complete(["special", "special2"])
        self.assertFalse(qm.completed(["special", "special2", "kill"]))


activator = Atrinik.WhoIsActivator()
me = Atrinik.WhoAmI()
suites = [
    unittest.TestLoader().loadTestsFromTestCase(QuestManagerSuite),
]
