## @file
## Gashir the bartender.

from QuestManager import QuestManagerMulti
from Interface import InterfaceBuilderQuest
from Quests import ShipmentOfCharobBeer as quest
from Tavern import Bartender

class InterfaceDialog(Bartender):
    """
    Default dialog when talking to the bartender.
    """

    def dialog_hello(self):
        self.add_msg("Welcome to Asterian Arms Tavern.")
        self.add_msg("I'm {npc.name}, the bartender of this tavern. Here is the place if you want to eat or drink the best booze! I can offer you the following provisions.")
        self.show_provisions()

    def can_buy(self, obj):
        if obj.arch.name == "beer_charob" and not self.qm.completed("deliver"):
            return False

        return True

    def show_bought(self, obj):
        self.add_msg("Here you go!")
        self.show_buy_icon(obj, buying = False)
        self.add_msg({
            "booze_generic": "Enjoy!",
            "booze2": "Please be careful though, it is really strong!",
            "food_generic": "It's really tasty, I tell you.",
            "drink_generic": "Thirsty? Nothing like fresh water!",
            "beer_charob": "It is quite good quality!",
        }[obj.arch.name])

class InterfaceDialog_need_start_deliver(InterfaceDialog):
    """
    Dialog when the player still needs to start the shipment quest.
    """

    def dialog_hello(self):
        InterfaceDialog.dialog_hello(self)
        self.add_msg("Unfortunately, it appears we are fresh out of our local specialty, the Charob Beer. If you want to buy some, you'll have to wait until the shipment arrives. In the meantime, maybe you could check down at the brewery to see what is holding my shipment up...")

class InterfaceDialog_need_finish_deliver(InterfaceDialog_need_start_deliver):
    """
    Dialog when the player still needs to finish the shipment quest; same
    as when needing to start it.
    """

class InterfaceDialog_need_complete_deliver(InterfaceDialog_need_start_deliver):
    """
    Dialog when the player needs to complete the shipment quest.
    """

    def dialog_hello(self):
        self.add_link("I have your shipment of Charob Beer.", dest = "shipment")
        InterfaceDialog_need_start_deliver.dialog_hello(self)

    def dialog_shipment(self):
        self.add_msg("Finally, I get my shipment of Charob Beer!")
        self.add_msg("You hand over the shipment of the Charob Beer.", color = COLOR_YELLOW)
        self.add_msg("Thank you! Now I can serve you with Charob Beer. I am sure you'll get a payment for your delivery too!")

        self.qm.start("reward")
        self.qm.complete("deliver")

qm = QuestManagerMulti(activator, quest)
ib = InterfaceBuilderQuest(activator, me)
ib.finish(locals(), qm, msg)
