import unittest
from collections import OrderedDict

import Atrinik
from tests import TestSuite, ib_wrapper
from QuestManager import QuestManager
from Interface import InterfaceBuilder


class InterfaceBuilderSuite(TestSuite):
    maxDiff = None

    def setUp(self):
        super().setUp()
        self.npc = activator.map.CreateObject("monk", activator.x, activator.y)
        self.npc.f_invulnerable = True
        self.npc.f_no_attack = True
        self.npc.Update()

    def tearDown(self):
        super().tearDown()
        self.npc.Destroy()

    def test_01(self):
        class InterfaceDialog(InterfaceBuilder):
            @ib_wrapper
            def dialog_hello(self):
                pass

            @ib_wrapper
            def dialog_how(self):
                pass

        ib = InterfaceBuilder(activator, self.npc)

        ib.finish(locals(), "hello")
        self.IB_test("InterfaceDialog.dialog_hello")
        ib.finish(locals(), "hi")
        self.IB_test(None)
        ib.finish(locals(), "how")
        self.IB_test("InterfaceDialog.dialog_how")

    def test_02(self):
        dialog_msg = None

        class InterfaceDialog(InterfaceBuilder):
            @ib_wrapper
            def dialog_hello(self):
                pass

            @ib_wrapper
            def dialog(self, msg):
                nonlocal dialog_msg
                dialog_msg = msg

        ib = InterfaceBuilder(activator, self.npc)

        ib.finish(locals(), "hello")
        self.IB_test("InterfaceDialog.dialog_hello")
        ib.finish(locals(), "hi")
        self.IB_test("InterfaceDialog.dialog")
        self.assertEqual(dialog_msg, "hi")
        ib.finish(locals(), "hello world")
        self.IB_test("InterfaceDialog.dialog")
        self.assertEqual(dialog_msg, "hello world")

    def test_03(self):
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

        # noinspection PyPep8Naming
        class InterfaceDialog_need_start_deliver(InterfaceBuilder):
            @ib_wrapper
            def dialog_hello(self):
                pass

            @ib_wrapper
            def dialog_how(self):
                pass

        # noinspection PyPep8Naming
        class InterfaceDialog_need_finish_deliver(InterfaceBuilder):
            @ib_wrapper
            def dialog_hello(self):
                pass

        # noinspection PyPep8Naming
        class InterfaceDialog_need_complete_deliver(InterfaceBuilder):
            @ib_wrapper
            def dialog_hello(self):
                pass

        # noinspection PyPep8Naming
        class InterfaceDialog_completed(InterfaceBuilder):
            @ib_wrapper
            def dialog_hello(self):
                pass

        ib = InterfaceBuilder(activator, self.npc)
        ib.set_quest(qm)

        ib.finish(locals(), "hello")
        self.IB_test("InterfaceDialog_need_start_deliver.dialog_hello")
        ib.finish(locals(), "hi")
        self.IB_test(None)
        ib.finish(locals(), "how")
        self.IB_test("InterfaceDialog_need_start_deliver.dialog_how")
        qm.start("deliver")
        ib.finish(locals(), "how")
        self.IB_test(None)
        ib.finish(locals(), "hello")
        self.IB_test("InterfaceDialog_need_finish_deliver.dialog_hello")
        sword = activator.CreateObject("sword")
        sword.name = "quest sword"
        ib.finish(locals(), "hello")
        self.IB_test("InterfaceDialog_need_complete_deliver.dialog_hello")
        qm.complete("deliver")
        ib.finish(locals(), "hello")
        self.IB_test("InterfaceDialog_completed.dialog_hello")

    def test_04(self):
        quest = {
            "parts": OrderedDict((("special", {
                "info": "",
                "uid": "special",
                "name": "Special",
            }), ("special2", {
                "info": "",
                "uid": "special2",
                "name": "Special 2",
                "parts": OrderedDict((("get_item", {
                    "info": "",
                    "uid": "get_item",
                    "name": "Get an item",
                    "item": {"arch": "sword", "name": "quest sword"},
                    "parts": OrderedDict((("get_item2", {
                        "info": "",
                        "uid": "get_item2",
                        "name": "Get an item 2",
                        "item": {"arch": "sword", "name": "special sword"},
                    }),)),
                }),)),
            }),)),
            "name": "Test Quest",
            "uid": "test_quest",
        }
        qm = QuestManager(activator, quest)

        # noinspection PyPep8Naming
        class InterfaceDialog_need_start_special(InterfaceBuilder):
            @ib_wrapper
            def dialog_hello(self):
                pass

        # noinspection PyPep8Naming
        class InterfaceDialog(InterfaceBuilder):
            @ib_wrapper
            def dialog_hello(self):
                pass

        # noinspection PyPep8Naming
        class InterfaceDialog_completed(InterfaceBuilder):
            @ib_wrapper
            def dialog_hello(self):
                pass

        # noinspection PyPep8Naming
        class InterfaceDialog2(InterfaceBuilder):
            @ib_wrapper
            def dialog_hello(self):
                pass

        # noinspection PyPep8Naming
        class InterfaceDialog2_completed(InterfaceBuilder):
            @ib_wrapper
            def dialog_hello(self):
                pass

        # noinspection PyPep8Naming
        class InterfaceDialog2_need_complete_special(InterfaceBuilder):
            @ib_wrapper
            def dialog_hello(self):
                pass

        # noinspection PyPep8Naming
        class InterfaceDialog2_need_complete_special2(InterfaceBuilder):
            @ib_wrapper
            def dialog_hello(self):
                pass

        # noinspection PyPep8Naming
        class InterfaceDialog2_need_finish_get_item(InterfaceBuilder):
            @ib_wrapper
            def dialog_hello(self):
                pass

        # noinspection PyPep8Naming
        class InterfaceDialog3(InterfaceBuilder):
            @ib_wrapper
            def dialog_hello(self):
                pass

        # noinspection PyPep8Naming
        class InterfaceDialog3_need_start_get_item2(InterfaceBuilder):
            @ib_wrapper
            def dialog_hello(self):
                pass

        # noinspection PyPep8Naming
        class InterfaceDialog3_need_finish_get_item2(InterfaceBuilder):
            @ib_wrapper
            def dialog_hello(self):
                pass

        # noinspection PyPep8Naming
        class InterfaceDialog3_need_complete_get_item2(InterfaceBuilder):
            @ib_wrapper
            def dialog_hello(self):
                pass

        ib = InterfaceBuilder(activator, self.npc)
        ib.set_quest(qm)

        ib.finish(locals(), "hello")
        self.IB_test("InterfaceDialog_need_start_special.dialog_hello")
        ib.finish(locals(), "hello", dialog_name="InterfaceDialog2")
        self.IB_test("InterfaceDialog2.dialog_hello")
        ib.finish(locals(), "hello", dialog_name="InterfaceDialog3")
        self.IB_test("InterfaceDialog3.dialog_hello")

        qm.start("special")
        ib.finish(locals(), "hello")
        self.IB_test("InterfaceDialog.dialog_hello")
        ib.finish(locals(), "hello", dialog_name="InterfaceDialog2")
        self.IB_test("InterfaceDialog2_need_complete_special.dialog_hello")
        ib.finish(locals(), "hello", dialog_name="InterfaceDialog3")
        self.IB_test("InterfaceDialog3.dialog_hello")

        qm.start("special2")
        qm.complete("special")
        ib.finish(locals(), "hello")
        self.IB_test("InterfaceDialog.dialog_hello")
        ib.finish(locals(), "hello", dialog_name="InterfaceDialog2")
        self.IB_test("InterfaceDialog2_need_complete_special2.dialog_hello")
        ib.finish(locals(), "hello", dialog_name="InterfaceDialog3")
        self.IB_test("InterfaceDialog3.dialog_hello")

        qm.start(["special2", "get_item"])
        ib.finish(locals(), "hello")
        self.IB_test("InterfaceDialog.dialog_hello")
        ib.finish(locals(), "hello", dialog_name="InterfaceDialog2")
        self.IB_test("InterfaceDialog2_need_finish_get_item.dialog_hello")
        ib.finish(locals(), "hello", dialog_name="InterfaceDialog3")
        self.IB_test("InterfaceDialog3_need_start_get_item2.dialog_hello")

        qm.start(["special2", "get_item", "get_item2"])
        ib.finish(locals(), "hello")
        self.IB_test("InterfaceDialog.dialog_hello")
        ib.finish(locals(), "hello", dialog_name="InterfaceDialog2")
        self.IB_test("InterfaceDialog2_need_finish_get_item.dialog_hello")
        ib.finish(locals(), "hello", dialog_name="InterfaceDialog3")
        self.IB_test("InterfaceDialog3_need_finish_get_item2.dialog_hello")

        sword = activator.CreateObject("sword")
        sword.name = "special sword"
        ib.finish(locals(), "hello")
        self.IB_test("InterfaceDialog.dialog_hello")
        ib.finish(locals(), "hello", dialog_name="InterfaceDialog2")
        self.IB_test("InterfaceDialog2_need_finish_get_item.dialog_hello")
        ib.finish(locals(), "hello", dialog_name="InterfaceDialog3")
        self.IB_test("InterfaceDialog3_need_complete_get_item2.dialog_hello")

        qm.complete(["special2", "get_item", "get_item2"])
        ib.finish(locals(), "hello")
        self.IB_test("InterfaceDialog.dialog_hello")
        ib.finish(locals(), "hello", dialog_name="InterfaceDialog2")
        self.IB_test("InterfaceDialog2_need_finish_get_item.dialog_hello")
        ib.finish(locals(), "hello", dialog_name="InterfaceDialog3")
        self.IB_test("InterfaceDialog3.dialog_hello")

        sword = activator.CreateObject("sword")
        sword.name = "quest sword"
        ib.finish(locals(), "hello")
        self.IB_test("InterfaceDialog.dialog_hello")
        ib.finish(locals(), "hello", dialog_name="InterfaceDialog2")
        self.IB_test("InterfaceDialog2_need_complete_special2.dialog_hello")
        ib.finish(locals(), "hello", dialog_name="InterfaceDialog3")
        self.IB_test("InterfaceDialog3.dialog_hello")

        qm.complete(["special2", "get_item"])
        ib.finish(locals(), "hello")
        self.IB_test("InterfaceDialog.dialog_hello")
        ib.finish(locals(), "hello", dialog_name="InterfaceDialog2")
        self.IB_test("InterfaceDialog2_need_complete_special2.dialog_hello")
        ib.finish(locals(), "hello", dialog_name="InterfaceDialog3")
        self.IB_test("InterfaceDialog3.dialog_hello")

        qm.complete("special2")
        ib.finish(locals(), "hello")
        self.IB_test("InterfaceDialog_completed.dialog_hello")
        ib.finish(locals(), "hello", dialog_name="InterfaceDialog2")
        self.IB_test("InterfaceDialog2_completed.dialog_hello")
        ib.finish(locals(), "hello", dialog_name="InterfaceDialog3")
        self.IB_test("InterfaceDialog3.dialog_hello")

    def test_05(self):
        class InterfaceDialog(InterfaceBuilder):
            @ib_wrapper
            def dialog_hello(self):
                pass

            @ib_wrapper
            def regex_dialog_how(self):
                pass

            @ib_wrapper
            def regex_dialog_why(self):
                pass

            matchers = [(r'how are you|how|how r u', regex_dialog_how),
                        (r'why (is|are|r) (u|you) here', regex_dialog_why)]

        ib = InterfaceBuilder(activator, self.npc)

        ib.finish(locals(), "hello")
        self.IB_test("InterfaceDialog.dialog_hello")
        ib.finish(locals(), "how are you")
        self.IB_test("InterfaceDialog.regex_dialog_how")
        ib.finish(locals(), "how")
        self.IB_test("InterfaceDialog.regex_dialog_how")
        ib.finish(locals(), "how r u")
        self.IB_test("InterfaceDialog.regex_dialog_how")
        ib.finish(locals(), "why r u here")
        self.IB_test("InterfaceDialog.regex_dialog_why")
        ib.finish(locals(), "why are u here")
        self.IB_test("InterfaceDialog.regex_dialog_why")
        ib.finish(locals(), "why r you here")
        self.IB_test("InterfaceDialog.regex_dialog_why")
        ib.finish(locals(), "why is you here")
        self.IB_test("InterfaceDialog.regex_dialog_why")

    def test_06(self):
        options = None

        # noinspection PyPep8Naming
        class InterfaceDialog_1(InterfaceBuilder):
            @ib_wrapper
            def dialog_hello(self):
                pass

        # noinspection PyPep8Naming
        class InterfaceDialog_2(InterfaceBuilder):
            @ib_wrapper
            def dialog_hello(self):
                pass

        def preconds(cls):
            nonlocal options
            if options == "interface1":
                cls.dialog_name = "InterfaceDialog_1"
            elif options == "interface2":
                cls.dialog_name = "InterfaceDialog_2"

        ib = InterfaceBuilder(activator, self.npc)
        ib.preconds = preconds

        options = "interface1"
        ib.finish(locals(), "hello")
        self.IB_test("InterfaceDialog_1.dialog_hello")

        options = "interface2"
        ib.finish(locals(), "hello")
        self.IB_test("InterfaceDialog_2.dialog_hello")

    def test_07(self):
        quest = {
            "parts": OrderedDict((("special", {
                "info": "",
                "uid": "special",
                "name": "Special",
                "parts": OrderedDict((("special1", {
                    "info": "",
                    "uid": "special1",
                    "name": "Get an item",
                }), ("special2", {
                    "info": "",
                    "uid": "special2",
                    "name": "Get an item",
                }), ("special3", {
                    "info": "",
                    "uid": "special3",
                    "name": "Get an item",
                }),)),
            }),)),
            "name": "Test Quest",
            "uid": "test_quest",
        }
        qm = QuestManager(activator, quest)

        # noinspection PyPep8Naming
        class InterfaceDialog_need_start_special(InterfaceBuilder):
            @ib_wrapper
            def dialog_hello(self):
                pass

        # noinspection PyPep8Naming
        class InterfaceDialog_need_complete_special(InterfaceBuilder):
            @ib_wrapper
            def dialog_hello(self):
                pass

        # noinspection PyPep8Naming
        class InterfaceDialog_completed(InterfaceBuilder):
            @ib_wrapper
            def dialog_hello(self):
                pass

        # noinspection PyPep8Naming
        class InterfaceDialog_need_complete_special1(
                InterfaceDialog_need_complete_special):
            pass

        # noinspection PyPep8Naming
        class InterfaceDialog_need_complete_special2(
                InterfaceDialog_need_complete_special):
            pass

        # noinspection PyPep8Naming
        class InterfaceDialog_need_complete_special3(
                InterfaceDialog_need_complete_special):
            pass

        ib = InterfaceBuilder(activator, self.npc)
        ib.set_quest(qm)

        ib.finish(locals(), "hello")
        self.IB_test("InterfaceDialog_need_start_special.dialog_hello")

        qm.start("special")
        qm.start(["special", "special1"])
        qm.start(["special", "special2"])
        qm.start(["special", "special3"])
        ib.finish(locals(), "hello")
        self.IB_test("InterfaceDialog_need_complete_special1.dialog_hello")

        qm.complete(["special", "special1"])
        ib.finish(locals(), "hello")
        self.IB_test("InterfaceDialog_need_complete_special2.dialog_hello")

        qm.complete(["special", "special2"])
        ib.finish(locals(), "hello")
        self.IB_test("InterfaceDialog_need_complete_special3.dialog_hello")

        qm.complete(["special", "special3"])
        ib.finish(locals(), "hello")
        self.IB_test("InterfaceDialog_need_complete_special.dialog_hello")

        qm.complete("special")
        ib.finish(locals(), "hello")
        self.IB_test("InterfaceDialog_completed.dialog_hello")


activator = Atrinik.WhoIsActivator()
me = Atrinik.WhoAmI()
suites = [
    unittest.TestLoader().loadTestsFromTestCase(InterfaceBuilderSuite),
]
