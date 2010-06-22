## @file
## Script used by the Spellgiver NPC in Developer Testmaps.

from Atrinik import *

activator = WhoIsActivator()
me = WhoAmI()

msg = WhatIsMessage().strip().lower()
words = msg.split()

# Learn a spell.
if words[0] == "learn":
	spell = GetSpellNr(msg[6:])

	# Invalid spell?
	if spell == -1:
		me.SayTo(activator, "\nUnknown spell.")
	else:
		if activator.DoKnowSpell(spell) == 1:
			me.SayTo(activator, "\nYou already know that spell.")
		else:
			activator.AcquireSpell(spell, LEARN)

# Unlearn a spell.
elif words[0] == "unlearn":
	spell = GetSpellNr(msg[8:])

	# Invalid spell?
	if spell == -1:
		me.SayTo(activator, "\nUnknown spell.")
	else:
		if activator.DoKnowSpell(spell) == 0:
			me.SayTo(activator, "\nYou don't know that spell.")
		else:
			activator.AcquireSpell(spell, UNLEARN)

else:
	me.SayTo(activator, "\nI am the Spellgiver.\nSay ^learn <spellname>^ or ^unlearn <spellname>^ to learn or unlearn a given spell.")
