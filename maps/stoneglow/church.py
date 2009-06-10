import Atrinik
import string

activator = Atrinik.WhoIsActivator()
whoami = Atrinik.WhoAmI()

msg = Atrinik.WhatIsMessage().strip().lower()
text = string.split(msg)

if msg == 'remove curse':
	if activator.PayAmount(1500) == 1:
		whoami.SayTo(activator, "\nOk, I will cast 'remove curse' for 15 silver on you.")
		activator.Write("You pay the money.", 0)
		whoami.CastAbility(activator, Atrinik.GetSpellNr("remove curse"), 1, 0, "")
	else:
		whoami.SayTo(activator, "\nSorry, you don't have enough money.")

elif msg == 'remove damnation':
	if activator.PayAmount(3000) == 1:
		whoami.SayTo(activator, "\nOk, I will cast 'remove damnation' for 30 silver on you.")
		activator.Write("You pay the money.", 0)
		whoami.CastAbility(activator, Atrinik.GetSpellNr("remove damnation"), 1, 0, "")
	else:
		whoami.SayTo(activator, "\nSorry, you don't have enough money.")

elif msg == 'remove depletion':
	if activator.level < 3:
		whoami.SayTo(activator, "\nYou are under level 3. I will help you for free this time!")
		whoami.CastAbility(activator, Atrinik.GetSpellNr("remove depletion"), 1, 0, "")
	else:
		if activator.PayAmount(3500) == 1:
			whoami.SayTo(activator, "\nOk, I will cast 'remove depletion' for 35 silver on you.")
			activator.Write("You pay the money.", 0)
			whoami.CastAbility(activator, Atrinik.GetSpellNr("remove depletion"), 1, 0, "")
		else:
			whoami.SayTo(activator,"\nSorry, you don't have enough money.")

elif msg == 'cure disease':
	if activator.level < 3:
		whoami.SayTo(activator, "\nYou are under level 3. I will help you for free this time!")
		whoami.CastAbility(activator, Atrinik.GetSpellNr("cure disease"), 1, 0, "")
	else:
		if activator.PayAmount(100) == 1:
			whoami.SayTo(activator, "\nOk, I will cast 'cure disease' for 1 silver on you.")
			activator.Write("You pay the money.", 0)
			whoami.CastAbility(activator, Atrinik.GetSpellNr("cure disease"), 1, 0, "")
		else:
			whoami.SayTo(activator, "\nSorry, you don't have enough money.")

elif msg == 'cure poison':
		whoami.SayTo(activator, "\nLet me help you ...")
		whoami.CastAbility(activator, Atrinik.GetSpellNr("cure poison"), 1, 0, "")

elif text[0] == "healing":
	spell = Atrinik.GetSpellNr("minor healing")
	if spell == -1:
		whoami.SayTo(activator, "Unknown spell.")
	else:
		if activator.DoKnowSpell(spell) == 1:
			whoami.SayTo(activator, "You already know this prayer...")
		else:	
			activator.AcquireSpell(spell, Atrinik.LEARN)

elif msg == 'wounds':
	spell = Atrinik.GetSpellNr("cause light wounds")
	if spell == -1:
		whoami.SayTo(activator, "Unknown spell.")
	else:
		if activator.DoKnowSpell(spell) == 1:
			whoami.SayTo(activator, "You already know this prayer...")
		else:	
			if activator.PayAmount(200) == 1:
				whoami.SayTo(activator, "\nOk, I will teach you now cause light wounds for 2 silver.")
				activator.Write("You pay the money.", 0)
				activator.AcquireSpell(spell, Atrinik.LEARN)
			else:
				whoami.SayTo(activator, "\nSorry, you don't have enough money.")

elif msg == 'food':
	if activator.food < 500:
		activator.food = 500
		whoami.SayTo(activator, "\nYour stomach is filled again.")
	else:
		whoami.SayTo(activator, "\nYou don't look very hungry to me...");

elif msg == 'hello' or msg == 'hi' or msg == 'hey':
	whoami.SayTo(activator,"\nWelcome to the church of the Tabernacle.\nYou can join the cult of Eldath here.\nRead in the book of deities what you have to do.\nI can ^remove curse^ for 15 silver, ^remove damnation^ for 30 silver\nor ^remove depletion^ for 35 silver.\nI will ^cure poison^ for free and ^cure disease^ for 1 silver.\nIf you are starving I can cast ^food^ on you.\nI can teach you minor ^healing^ for free and cause light ^wounds^ for 2 silver!"); 

