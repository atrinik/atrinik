## @file
## Implements Steve Bruck from the Charob Beer warehouse.

from Interface import InterfaceBuilderQuest
from QuestManager import QuestManagerMulti
from Quests import ShipmentOfCharobBeer as quest

class InterfaceDialog(InterfaceBuilderQuest):
    """
    Default dialog for Steve Bruck.
    """

    def dialog_hello(self):
        self.add_msg("Welcome to Charob Beer's Shipping Department. I am as usual overworked and underpaid.")

class InterfaceDialog_completed(InterfaceBuilderQuest):
    """
    Tell the player to come back soon when the quest has been completed,
    as this is a repeat quest.
    """

    def dialog_hello(self):
        InterfaceDialog.dialog_hello(self)
        self.add_msg("Thank you for the help with that shipment. I'll tell you when I need you for another delivery.")

class InterfaceDialog_need_start_deliver(InterfaceBuilderQuest):
    """
    Offer the quest to the player.
    """

    def dialog_hello(self):
        InterfaceDialog.dialog_hello(self)
        self.add_msg("I also have this shipment of beer gathering dust here.")
        self.add_link("Tell me more about this shipment.", dest = "moreinfo")

    def dialog_moreinfo(self):
        self.add_msg("My body is terribly sore, and I've got a lot of shipments to sort. So I need someone to deliver this shipment of Charob Beer to the Asterian Arms Tavern.")
        self.add_link("I can handle that.", dest = "handle")

    def dialog_handle(self):
        self.add_msg("Ah, you would? Great! You can find the shipment in question in a chest in the other room. Please deliver it to Gashir at the Asterian Arms Tavern.")

        self.qm.start("deliver")

class InterfaceDialog_need_complete_deliver(InterfaceBuilderQuest):
    """
    Dialog for when the delivery has not been made yet.
    """

    def dialog_hello(self):
        InterfaceDialog.dialog_hello(self)
        self.add_msg("Well, have you delivered the shipment to Gashir at the Asterian Arms Tavern yet?")
        self.add_link("Working on it.", dest = "working")

    def dialog_working(self):
        self.add_msg("Well, hurry up and get that beer to Gashir before I lose my job!")

class InterfaceDialog_need_finish_deliver(InterfaceDialog_need_complete_deliver):
    """
    Dialog for when the delivery has not been made yet, and the player
    does not have the shipment in their inventory yet.
    """

    def dialog_hello(self):
        InterfaceDialog_need_complete_deliver.dialog_hello(self)
        self.add_link("Where can I find the shipment?", dest = "shipment")

    def dialog_shipment(self):
        self.add_msg("You can find the shipment in a chest in the other room.")

class InterfaceDialog_need_complete_reward(InterfaceDialog):
    """
    Dialog when the player has made the delivery, give out the reward.
    """

    def dialog_hello(self):
        InterfaceDialog.dialog_hello(self)
        self.add_msg("Well, have you delivered the shipment to Gashir at the Asterian Arms Tavern yet?")
        self.add_link("Yes.", dest = "yes")

    def dialog_yes(self):
        self.add_msg("{npc.name} whistles at you appreciatively.", color = COLOR_YELLOW)
        self.add_msg("Thank you! I hope he wasn't too upset about the late delivery!")
        self.add_msg("Here is your payment...")
        self.add_objects(me.FindObject(archname = "silvercoin"))

        self.qm.complete("reward")

qm = QuestManagerMulti(activator, quest)
ib = InterfaceBuilderQuest(activator, me)
ib.finish(locals(), qm, msg)
