## @file
## Generic script for smiths in shops, to identify items, detect curse,
## etc.

from Atrinik import *
import string

## Activator object.
activator = WhoIsActivator()
## Object who has the event object in their inventory.
me = WhoAmI()

msg = WhatIsMessage().strip().lower()
text = string.split(msg)

## Dictionary of all possible costs of services. Based on player's level.
costs = {
	"detect_curse": 25 + (25 * activator.level),
	"detect_magic": 25 + (25 * activator.level),
	"identify": 50 + (10 * activator.level),
	"identify_all": 200 + (50 * activator.level),
}

## If the activator is under this level, some of the services are free.
level_free = 5

# Detect curse on all items
if msg == "detect curse":
	if activator.level < level_free:
		me.SayTo(activator, "\nYou are under level %d. I will do this service for free this time!" % level_free)
		me.CastAbility(activator, GetSpellNr("detect curse"), 1, 0, "")
	else:
		if activator.PayAmount(costs["detect_curse"]) == 1:
			me.SayTo(activator, "\nOk, I will detect curse on your items.")
			activator.Write("You pay the money.", 0)
			me.CastAbility(activator, GetSpellNr("detect curse"), 1, 0, "")
		else:
			me.SayTo(activator, "\nSorry, you do not have enough money.")

# Detect magic on all items
elif msg == "detect magic":
	if activator.level < level_free:
		me.SayTo(activator, "\nYou are under level %d. I will do this service for free this time!" % level_free)
		me.CastAbility(activator, GetSpellNr("detect magic"), 1, 0, "")
	else:
		if activator.PayAmount(costs["detect_magic"]) == 1:
			me.SayTo(activator, "\nOk, I will detect magic on your items.")
			activator.Write("You pay the money.", 0)
			me.CastAbility(activator, GetSpellNr("detect magic"), 1, 0, "")
		else:
			me.SayTo(activator, "\nSorry, you do not have enough money.")

# Identify a single marked item
elif msg == "identify":
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
		"detect_curse": activator.ShowCost(costs["detect_curse"]),
		"detect_magic": activator.ShowCost(costs["detect_magic"]),
		"identify": activator.ShowCost(costs["identify"]),
		"identify_all": activator.ShowCost(costs["identify_all"]),
	}

	# Some of the services are free if the player is under level three
	if activator.level < level_free:
		cost_strings["detect_curse"] = "free"
		cost_strings["detect_magic"] = "free"
		cost_strings["identify"] = "free"
		cost_strings["identify_all"] = "free"

	me.SayTo(activator, "\nWelcome to my shop. We have what you want!\nI can offer you the following services:\n^detect curse^ for %s\n^detect magic^ for %s\n^identify^ for %s\n^identify all^ for %s." % (cost_strings["detect_curse"], cost_strings["detect_magic"], cost_strings["identify"], cost_strings["identify_all"]))
