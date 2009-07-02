import Atrinik
import string

activator=Atrinik.WhoIsActivator()
whoami=Atrinik.WhoAmI()

msg = Atrinik.WhatIsMessage().strip().lower()
text = string.split(msg)

if text[0] == "learn":
	spell = Atrinik.GetSpellNr(msg[5:].strip())
	if spell == -1:
		whoami.SayTo(activator,"Unknown spell." )
	else:
		if activator.DoKnowSpell(spell) == 1:
			whoami.SayTo(activator,"You already learned this spell." )
		else:	
			activator.AcquireSpell(spell, Atrinik.LEARN )

elif text[0] == "unlearn":
	spell = Atrinik.GetSpellNr(msg[7:].strip())
	if spell == -1:
		whoami.SayTo(activator,"Unknown spell.")
	else:
		if activator.DoKnowSpell(spell) == 0:
			whoami.SayTo(activator,"You don't know this spell." )
		else:	
			activator.AcquireSpell(spell, Atrinik.UNLEARN )

elif msg == 'food':
	activator.food = 999
	whoami.SayTo(activator,'\nYour stomach is filled again.')

else:
	whoami.SayTo(activator,"\nI am the Spellgiver.\nSay ^learn <spellname>^ or ^unlearn <spellname>^")
