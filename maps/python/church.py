# Generic script for church priests. Includes removing curse, damnation,
# curing poison, etc.
import Atrinik
import string

activator = Atrinik.WhoIsActivator()
me = Atrinik.WhoAmI()

msg = Atrinik.WhatIsMessage().strip().lower()
text = string.split(msg)

# All possible costs of services. Based on player's level.
costs = {
	"remove_curse": 100 * activator.level,
	"remove_damnation": 100 + (3 * activator.level * activator.level),
	"remove_depletion": 100 + (4 * activator.level * activator.level),
	"cure_disease": 100 * activator.level,
	"cure_poison": 5 * activator.level,
}

# Remove curse
if msg == "remove curse":
	if activator.PayAmount(costs["remove_curse"]) == 1:
		me.SayTo(activator, "\nOk, I will cast remove curse on you now.")
		activator.Write("You pay the money.", 0)
		me.CastAbility(activator, Atrinik.GetSpellNr("remove curse"), 1, 0, "")
	else:
		me.SayTo(activator, "\nSorry, you don't have enough money.")

# Remove damnation
elif msg == "remove damnation":
	if activator.PayAmount(costs["remove_damnation"]) == 1:
		me.SayTo(activator, "\nOk, I will cast remove damnation on you now.")
		activator.Write("You pay the money.", 0)
		me.CastAbility(activator, Atrinik.GetSpellNr("remove damnation"), 1, 0, "")
	else:
		me.SayTo(activator, "\nSorry, you don't have enough money.")

# Remove depletion
elif msg == "remove depletion":
	if activator.level < 3:
		me.SayTo(activator, "\nYou are under level 3. I will help you for free this time!")
		me.CastAbility(activator, Atrinik.GetSpellNr("remove depletion"), 1, 0, "")
	else:
		if activator.PayAmount(costs["remove_depletion"]) == 1:
			me.SayTo(activator, "\nOk, I will cast remove depletion on you now.")
			activator.Write("You pay the money.", 0)
			me.CastAbility(activator, Atrinik.GetSpellNr("remove depletion"), 1, 0, "")
		else:
			me.SayTo(activator,"\nSorry, you don't have enough money.")

# Cure disease
elif msg == "cure disease":
	if activator.level < 3:
		me.SayTo(activator, "\nYou are under level 3. I will help you for free this time!")
		me.CastAbility(activator, Atrinik.GetSpellNr("cure disease"), 1, 0, "")
	else:
		if activator.PayAmount(costs["cure_disease"]) == 1:
			me.SayTo(activator, "\nOk, I will cast cure disease on you now.")
			activator.Write("You pay the money.", 0)
			me.CastAbility(activator, Atrinik.GetSpellNr("cure disease"), 1, 0, "")
		else:
			me.SayTo(activator, "\nSorry, you don't have enough money.")

# Cure poison
elif msg == "cure poison":
	if activator.level < 3:
		me.SayTo(activator, "\nYou are under level 3. I will help you for free this time!")
		me.CastAbility(activator, Atrinik.GetSpellNr("cure poison"), 1, 0, "")
	else:
		if activator.PayAmount(costs["cure_poison"]) == 1:
			me.SayTo(activator, "\nOk, I will cast cure poison on you now.")
			activator.Write("You pay the money.", 0)
			me.CastAbility(activator, Atrinik.GetSpellNr("cure poison"), 1, 0, "")
		else:
			me.SayTo(activator, "\nSorry, you don't have enough money.")

# Give out food
elif msg == "food":
	if activator.food < 500:
		activator.food = 500
		me.SayTo(activator, "\nYour stomach is filled again.")
	else:
		me.SayTo(activator, "\nYou don't look very hungry to me...");

# Greeting
elif msg == "hello" or msg == "hi" or msg == "hey":
	# Cost strings that might need to be modified before being displayed
	cost_strings = {
		"remove_depletion": activator.ShowCost(costs["remove_depletion"]),
		"cure_disease": activator.ShowCost(costs["cure_disease"]),
		"cure_poison": activator.ShowCost(costs["cure_poison"]),
	}

	# Some of the services are free if the player is under level three
	if activator.level < 3:
		cost_strings["remove_depletion"] = "free"
		cost_strings["cure_disease"] = "free"
		cost_strings["cure_poison"] = "free"

	me.SayTo(activator, "\nWelcome to the church of the Tabernacle.\nYou can join the cult of Eldath here.\nRead in the book of deities what you have to do.\nI can offer you the following services:\n^remove curse^ for %s\n^remove damnation^ for %s\n^remove depletion^ for %s\n^cure disease^ for %s\n^cure poison^ for %s\nIf you are starving I can give you some ^food^." % (activator.ShowCost(costs["remove_curse"]), activator.ShowCost(costs["remove_damnation"]), cost_strings["remove_depletion"], cost_strings["cure_disease"], cost_strings["cure_poison"]));
