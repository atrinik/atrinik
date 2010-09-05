## @file
## Implements Gashir, the owner of the Asterian Arms Tavern and Steve
## Bruck from the Charob Beer warehouse.

from Atrinik import *
from QuestManager import QuestManager

activator = WhoIsActivator()
me = WhoAmI()
msg = WhatIsMessage().strip().lower()

## Info about the quest.
quest = {
	"quest_name": "Shipment of Charob Beer",
	"type": QUEST_TYPE_KILL_ITEM,
	"arch_name": "barrel2.101",
	"item_name": "shipment of Charob Beer",
	"message": "Deliver Charob Beer to the Asterian Arms Tavern.",
}

qm = QuestManager(activator, quest)

def npc_gashir():
	if msg == "hello" or msg == "hi" or msg == "hey":
		if qm.finished():
			me.Communicate("/smile")
			me.SayTo(activator, "\nFinally, I get my shipment of Charob Beer!")
			activator.Write("You give the {0} to {1}.".format(quest["item_name"], me.name), COLOR_WHITE)
			me.SayTo(activator, "Thank you! Now I can serve you with ^Charob Beer^ for 8 copper. I am sure you'll get a nice reward for your delivery too!", 1)
			qm.complete()
		else:
			me.SayTo(activator, "\nWelcome to Asterian Arms Tavern\nI'm Gashir, the bartender of this tavern. Here is the place if you want to eat or drink the best booze! I can sell you ^booze^ for just 5 copper, really ^strong booze^ for 10 copper or ^food^ for 10 copper. We have also some ^water^, for 2 copper, and finally our local special, ^Charob Beer^, for 8 copper. Also, please, do not ^complain^ about quality of strong booze.")

	elif msg == "complain":
		me.SayTo(activator, "\nI told you it is really strong! What did you expect? Elvish wine?")

	elif msg == "this ale stinks":
		me.SayTo(activator, "\nIf you don't like it, find another tavern!")

	elif msg == "booze":
		if activator.PayAmount(5):
			me.SayTo(activator, "\nHere you go! Enjoy!")
			activator.CreateObjectInside("booze_generic", 1, 1, 1)
			activator.Write("You pay the money.", 0)
		else:
			me.SayTo(activator, "\nSorry, you do not have enough money.")

	elif msg == "strong booze":
		if activator.PayAmount(10):
			me.SayTo(activator, "\nHere you go! But be careful, it is really strong!")
			activator.CreateObjectInside("booze2", 1, 1, 1)
			activator.Write("You pay the money.", 0)
		else:
			me.SayTo(activator, "\nSorry, you do not have enough money.")

	elif msg == "charob beer":
		if not qm.completed():
			me.SayTo(activator, "\nAh, sorry. We are fresh out! Maybe you could check down at the brewery to see what is holding my shipment up?")
		elif activator.PayAmount(8):
			me.SayTo(activator, "\nHere you go! It is quite good quality!")
			me.CheckInventory(0, "beer").Clone().InsertInside(activator)
			activator.Write("You pay the money.", 0)
		else:
			me.SayTo(activator, "\nSorry, you do not have enough money.")

	elif msg == "water":
		if activator.PayAmount(2):
			me.SayTo(activator, "\nThirsty? Nothing like fresh water!")
			activator.CreateObjectInside("drink_generic", 1, 1, 1)
			activator.Write("You pay the money.", 0)
		else:
			me.SayTo(activator, "\nSorry, you do not have enough money.")

	elif msg == "food":
		if activator.PayAmount(10):
			me.SayTo(activator, "\nHere you go! It's really tasty, I tell you.")
			activator.CreateObjectInside("food_generic", 1, 1, 1)
			activator.Write("You pay the money.", 0)
		else:
			me.SayTo(activator, "\nSorry, you do not have enough money.")

def npc_steve():
	if msg == "hello" or msg == "hi" or msg == "hey":
		me.SayTo(activator, "\nWelcome to Charob Beer's Shipping Department.\nI am as usual overworked and underpaid.")

		if not qm.started():
			me.SayTo(activator, "I also have this ^shipment^ of beer gathering dust here.", 1)
		elif qm.completed():
			if qm.quest_object.hp:
				me.SayTo(activator, "Thanks for the help with that shipment, I should have another one soon.", 1)
			else:
				me.SayTo(activator, "|{0} smiles at you appreciatively.|\nThank you! I hope he wasn't too upset about the late delivery!".format(me.name), 1)
				me.SayTo(activator, "Here is your payment...", 1)
				activator.CreateObjectInside("silvercoin", 1, 5)
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

if me.name == "Gashir":
	npc_gashir()
else:
	npc_steve()
