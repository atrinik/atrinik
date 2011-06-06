## @file
## Script for the Spells Teacher, who informs the player about spells
## they can learn and the spells the player knows, with the ability to
## learn or forget any spell.

from Atrinik import *

activator = WhoIsActivator()
me = WhoAmI()
msg = WhatIsMessage().strip().lower()

def main():
	if msg == "hi" or msg == "hey" or msg == "hello":
		me.SayTo(activator, "\nHello. I am {} -- I can teach you any spell if you say <a>learn spell</a>, or make you forget a spell if you say <a>forget spell</a>. I can also show you <a>available</a> spells and the ones you have <a>learned</a>.".format(me.name))

	# Show available spells.
	elif msg == "available":
		me.SayTo(activator, "\nSpells you can learn (click on any to learn them): {}".format(", ".join("<a=:learn {0}>{0}</a>".format(GetSpell(i)["name"]) for i in range(NROFREALSPELLS) if not i in activator.Controller().known_spells)))

	# Show only known spells/
	elif msg == "learned":
		me.SayTo(activator, "\nSpells you know (click on any to unlearn them): {}".format(", ".join("<a=:forget {0}>{0}</a>".format(GetSpell(i)["name"]) for i in activator.Controller().known_spells)))

	# Learn a spell.
	elif msg.startswith("learn "):
		spell = GetSpellNr(msg[6:])

		if spell == -1:
			me.SayTo(activator, "\nUnknown spell.")
			return

		if activator.Controller().DoKnowSpell(spell):
			me.SayTo(activator, "\nYou already know that spell.")
			return

		# Learn the spell.
		activator.Controller().AcquireSpell(spell)

	# Forget a spell.
	elif msg.startswith("forget "):
		spell = GetSpellNr(msg[7:])

		if spell == -1:
			me.SayTo(activator, "\nUnknown spell.")
			return

		if not activator.Controller().DoKnowSpell(spell):
			me.SayTo(activator, "\nYou don't know that spell.")
			return

		# Forget the spell.
		activator.Controller().AcquireSpell(spell, False)

main()
