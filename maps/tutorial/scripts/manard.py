from Atrinik import *

activator = WhoIsActivator()
me = WhoAmI()
msg = WhatIsMessage().strip().lower()

def main():
	if activator.GetGod() != "Tabernacle":
		if msg == "hello" or msg == "hi" or msg == "hey":
			me.SayTo(activator, "\nWelcome to the church of the Tabernacle.\nTo access the powers of Tabernacle, you have to apply an altar of Tabernacle. One is over there, east of me.\nStep over it and apply it. Then come back to me.")

		return

	if msg == "hello" or msg == "hi" or msg == "hey":
		if activator.Controller().DoKnowSpell(GetSpellNr("minor healing")):
			me.SayTo(activator, "\nWelcome back. Do you need information how to <a>cast</a> prayers and spells?")
			return

		me.SayTo(activator, "\nVery good. Now, you will need the <a>minor healing</a> prayer in your adventures.")

	elif msg == "minor healing":
		sp = GetSpellNr("minor healing")

		if activator.Controller().DoKnowSpell(sp):
			me.SayTo(activator, "\nYou already know this prayer. Do you perhaps need information how to <a>cast</a> it?")
			return

		activator.Controller().AcquireSpell(sp)
		me.SayTo(activator, "\nExcellent. The <green>minor healing</green> prayer will heal your HP -- the number of hit points it heals increases with your <green>Divine Prayers</green> skill level. Note that if you have a friendly creature as your target (myself, for example), when you cast minor healing it will heal the target instead of you (but you can never heal enemies).\nNow, you should learn how to <a>cast</a> prayers and spells.")

	elif msg == "cast":
		me.SayTo(activator, "\nTo cast a prayer you need a deity -- you're already a follower of the Tabernacle. You can see this under your character's name at top left.\nYou can cast a spell or prayer in two ways:\nYou can type <green>/cast spell name</green> in the input console (same as the one used to talk to NPCs) -- for example, <green>/cast minor healing</green>.\nOr you can select the spell menu with F9 (or clicking <green>Spells</green> button at top right), going to the entry minor healing and pressing return over it. Then you can use it in the range chooser like throwing.")

main()
