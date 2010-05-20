## @file
## Script for the spell teacher NPC in Wizards' Tower
## in Lyondale Archipelago.

from Atrinik import *

activator = WhoIsActivator()
me = WhoAmI()

msg = WhatIsMessage().strip().lower()

## The available spells a player can buy.
spells = [
	{
		"spell": "transform wealth",
		"msg": "Transform wealth is a spell that will transform your marked copper coins into silver coins and silver coins into gold coins, at 20% sacrifice.",
		"cost": 30000,
	},
]

def main():
	if msg == "hello" or msg == "hi" or msg == "hey":
		me.SayTo(activator, "\nHello, I am {0} the Thelras' spell teacher. I can teach you various spells, for a fee. I can offer you the following spells:\n{1}".format(me.name, ", ".join(map(lambda d: "^" + d["spell"] + "^", spells))))

	# Do we want to buy the spell?
	elif msg[:6] == "learn ":
		# Try to find the spell we want to learn.
		l = list(filter(lambda d: d["spell"] == msg[6:], spells))

		# No such spell?
		if not l:
			return

		sp = l[0]
		skill = GetSkillNr("wizardry spells")

		if skill == -1:
			me.SayTo(activator, "Unknown skill.")
			return

		# Make sure we know the wizardry skill.
		if not activator.DoKnowSkill(skill):
			me.SayTo(activator, "\nYou first need the wizardry spells skill to learn spells!")
			return

		sp_nr = GetSpellNr(sp["spell"])

		# Do we know the spell already?
		if activator.DoKnowSpell(sp_nr):
			me.SayTo(activator, "\nYou already know the spell {0}.".format(sp["spell"]))
			return

		if activator.PayAmount(sp["cost"]) == 1:
			activator.Write("\nYou hand over {0} to {1}.".format(me.ShowCost(sp["cost"]), me.name), COLOR_YELLOW)
			me.SayTo(activator, "\nHere we go!")
			activator.AcquireSpell(sp_nr, LEARN)
		else:
			me.SayTo(activator, "\nSorry, you do not have enough money.")

	else:
		# Message didn't match any of the above, so try to find a spell.
		l = list(filter(lambda d: d["spell"] == msg, spells))

		# No such spell?
		if not l:
			return

		sp = l[0]
		me.SayTo(activator, "\n{0}\n{1} will cost you {2}. Do you want to ^learn {3}^?".format(sp["msg"], sp["spell"].capitalize(), me.ShowCost(sp["cost"]), sp["spell"]))


main()
