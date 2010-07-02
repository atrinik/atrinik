## @file
## Generic script for smiths in shops.

from Atrinik import *

activator = WhoIsActivator()
me = WhoAmI()

msg = WhatIsMessage().strip().lower()
text = msg.split()

## Dictionary of all possible costs of services. Based on player's level.
costs = {
	"identify": 50 + (10 * activator.level),
	"identify_all": 200 + (50 * activator.level),
}

## If the activator is under this level, some of the services are free.
level_free = 5

def main():
	if msg == "hello" or msg == "hi" or msg == "hey":
		## Cost strings.
		cost_strings = {
			"identify": activator.ShowCost(costs["identify"]),
			"identify_all": activator.ShowCost(costs["identify_all"]),
		}

		# Some of the services are free if the player is under certain level
		if activator.level < level_free:
			cost_strings["identify"] = "free"
			cost_strings["identify_all"] = "free"

		me.SayTo(activator, "\nWelcome to my shop. We have what you want!\nI can offer you the following ^services^:\n^identify^ for {0}\n^identify all^ for {1}.".format(cost_strings["identify"], cost_strings["identify_all"]))

	# Identify a single marked item
	elif msg == "identify":
		## Get the marked object.
		marked_object = activator.FindMarkedObject()

		if marked_object == None:
			me.SayTo(activator, "\nYou must mark an item first.")
		else:
			if activator.level < level_free:
				me.SayTo(activator, "\nYou are under level {0}. I will do this service for free this time!".format(level_free))
				me.IdentifyItem(activator, IDENTIFY_MARKED, marked_object)
			else:
				if activator.PayAmount(costs["identify"]) == 1:
					me.SayTo(activator, "\nOk, I will identify the {0}.".format(marked_object.name))
					activator.Write("You pay the money.", 0)
					me.IdentifyItem(activator, IDENTIFY_MARKED, marked_object)
				else:
					me.SayTo(activator, "\nSorry, you do not have enough money.")

	# Identify all items
	elif msg == "identify all":
		if activator.level < level_free:
			me.SayTo(activator, "\nYou are under level {0}. I will do this service for free this time!".format(level_free))
			me.IdentifyItem(activator, IDENTIFY_ALL, activator.FindMarkedObject())
		else:
			if activator.PayAmount(costs["identify_all"]) == 1:
				me.SayTo(activator, "\nOk, I will identify all your items.")
				activator.Write("You pay the money.", 0)
				me.IdentifyItem(activator, IDENTIFY_ALL, activator.FindMarkedObject())
			else:
				me.SayTo(activator, "\nSorry, you do not have enough money.")

	elif msg == "services":
		me.SayTo(activator, "\n~identify~ will identify only one item, the one which you have marked with |M|.\n~identify all~ will identify all items in your inventory, or if you have a marked container, all items inside it.")

main()
