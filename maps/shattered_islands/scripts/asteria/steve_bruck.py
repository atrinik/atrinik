## @file
## Implements Steve Bruck from the Charob Beer warehouse.

from Atrinik import *
from QuestManager import QuestManager
from Quests import ShipmentOfCharobBeer as quest

activator = WhoIsActivator()
me = WhoAmI()
msg = WhatIsMessage().strip().lower()
qm = QuestManager(activator, quest)

def main():
	if msg == "hello" or msg == "hi" or msg == "hey":
		me.SayTo(activator, "\nWelcome to Charob Beer's Shipping Department.\nI am as usual overworked and underpaid.")

		if not qm.started():
			me.SayTo(activator, "I also have this <a>shipment</a> of beer gathering dust here.", 1)
		elif qm.completed():
			if qm.quest_object.hp:
				me.SayTo(activator, "Thanks for the help with that shipment, I should have another one soon.", 1)
			else:
				me.SayTo(activator, "<yellow>{0} smiles at you appreciatively.</yellow>\nThank you! I hope he wasn't too upset about the late delivery!".format(me.name), 1)
				me.SayTo(activator, "Here is your payment...", 1)
				activator.CreateObject("silvercoin", 5)
				activator.Write("You received 5 silver coins.", COLOR_WHITE)
				# Mark that we have received the reward.
				qm.quest_object.hp = 1
		elif not qm.finished():
			me.SayTo(activator, "You can find the beer shipment in a chest in the other room.", 1)
		else:
			me.SayTo(activator, "Hurry up and get that beer to Gashir before I lose my job!", 1)

	elif msg == "shipment":
		if not qm.started():
			me.SayTo(activator, "\nGreat! My body is terribly sore, and I've got a lot of shipments to sort. You can find the shipment in question in a chest in the other room. Please deliver it to Gashir at the Asterian Arms Tavern.")
			qm.start()

main()
