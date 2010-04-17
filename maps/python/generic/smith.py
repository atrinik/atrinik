## @file
## Generic script for smiths in shops, to identify items.

from Atrinik import *

## Activator object.
activator = WhoIsActivator()
## Object who has the event object in their inventory.
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

# Identify a single marked item
if msg == "identify":
	## Get the marked object.
	marked_object = activator.FindMarkedObject()

	if marked_object == None:
		me.SayTo(activator, "\nYou must mark an item first.")
	else:
		if activator.level < level_free:
			me.SayTo(activator, "\nYou are under level %d. I will do this service for free this time!" % level_free)
			me.IdentifyItem(activator, marked_object, IDENTIFY_MARKED)
		else:
			if activator.PayAmount(costs["identify"]) == 1:
				me.SayTo(activator, "\nOk, I will identify the %s." % marked_object.name)
				activator.Write("You pay the money.", 0)
				me.IdentifyItem(activator, marked_object, IDENTIFY_MARKED)
			else:
				me.SayTo(activator, "\nSorry, you do not have enough money.")

# Identify all items
elif msg == "identify all":
	if activator.level < level_free:
		me.SayTo(activator, "\nYou are under level %d. I will do this service for free this time!" % level_free)
		me.IdentifyItem(activator, None, IDENTIFY_ALL)
	else:
		if activator.PayAmount(costs["identify_all"]) == 1:
			me.SayTo(activator, "\nOk, I will identify all your items.")
			activator.Write("You pay the money.", 0)
			me.IdentifyItem(activator, None, IDENTIFY_ALL)
		else:
			me.SayTo(activator, "\nSorry, you do not have enough money.")

# Greeting
elif msg == "hello" or msg == "hi" or msg == "hey":
	## Cost strings.
	cost_strings = {
		"identify": activator.ShowCost(costs["identify"]),
		"identify_all": activator.ShowCost(costs["identify_all"]),
	}

	# Some of the services are free if the player is under certain level
	if activator.level < level_free:
		cost_strings["identify"] = "free"
		cost_strings["identify_all"] = "free"

	me.SayTo(activator, "\nWelcome to my shop. We have what you want!\nI can offer you the following services:\n^identify^ for %s\n^identify all^ for %s." % (cost_strings["identify"], cost_strings["identify_all"]))
