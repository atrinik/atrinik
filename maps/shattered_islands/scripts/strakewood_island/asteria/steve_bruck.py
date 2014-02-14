## @file
## Implements Steve Bruck from the Charob Beer warehouse.

from Interface import Interface
from QuestManager import QuestManagerMulti
from Quests import ShipmentOfCharobBeer as quest

inf = Interface(activator, me)
qm = QuestManagerMulti(activator, quest)

def main():
    if msg == "hello":
        inf.add_msg("Welcome to Charob Beer's Shipping Department. I am as usual overworked and underpaid.")

    if not qm.started("deliver"):
        if msg == "hello":
            inf.add_msg("I also have this shipment of beer gathering dust here.")
            inf.add_link("Tell me more about this shipment.", dest = "moreinfo")
        elif msg == "moreinfo":
            inf.add_msg("My body is terribly sore, and I've got a lot of shipments to sort. So I need someone to deliver this shipment of Charob Beer to the Asterian Arms Tavern.")
            inf.add_link("I can handle that.", dest = "handle")
        elif msg == "handle":
            inf.add_msg("Ah, you would? Great! You can find the shipment in question in a chest in the other room. Please deliver it to Gashir at the Asterian Arms Tavern.")
            qm.start("deliver")
    elif not qm.completed("deliver"):
        if msg == "hello":
            inf.add_msg("Well, have you delivered the shipment to Gashir at the Asterian Arms Tavern yet?")

            if not qm.finished("deliver"):
                inf.add_msg("You can find the shipment in a chest in the other room.")

            inf.add_link("Working on it.", dest = "working")
        elif msg == "working":
            inf.add_msg("Well, hurry up and get that beer to Gashir before I lose my job!")
    elif qm.need_complete("reward"):
        if msg == "hello":
            inf.add_msg("Well, have you delivered the shipment to Gashir at the Asterian Arms Tavern yet?")
            inf.add_link("Yes.", dest = "yes")

        elif msg == "yes":
            inf.add_msg("{} whistles at you appreciatively.".format(me.name), COLOR_YELLOW)
            inf.add_msg("Thank you! I hope he wasn't too upset about the late delivery!")
            inf.add_msg("Here is your payment...")
            inf.add_objects(me.FindObject(archname = "silvercoin"))
            qm.complete()

    else:
        inf.add_msg("Thank you for the help with that shipment. I'll tell you when I need you for another delivery.")

main()
inf.finish()
