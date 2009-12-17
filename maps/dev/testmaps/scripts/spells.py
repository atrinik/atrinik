## @file
## Script used by the Spellgiver NPC in Developer Testmaps.

from Atrinik import *

## Activator object.
activator = WhoIsActivator()
## Object who has the event object in their inventory.
me = WhoAmI()

msg = WhatIsMessage().strip().lower()
text = msg.split()

# Learn a spell.
if text[0] == "learn":
	## Get the spell number.
	spell = GetSpellNr(text[1])

	# Invalid spell?
	if spell == -1:
		me.SayTo(activator, "Unknown spell.")
	else:
		if activator.DoKnowSpell(spell) == 1:
			me.SayTo(activator, "You already know that spell.")
		else:
			activator.AcquireSpell(spell, LEARN)

# Unlearn a spell.
elif text[0] == "unlearn":
	spell = GetSpellNr(text[1])

	# Invalid spell?
	if spell == -1:
		me.SayTo(activator, "Unknown spell.")
	else:
		if activator.DoKnowSpell(spell) == 0:
			me.SayTo(activator, "You don't know that spell.")
		else:
			activator.AcquireSpell(spell, UNLEARN)

else:
	me.SayTo(activator, "\nI am the Spellgiver.\nSay ^learn <spellname>^ or ^unlearn <spellname>^ to learn or unlearn a given spell.")
