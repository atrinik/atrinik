import Atrinik
import string

activator=Atrinik.WhoIsActivator()
whoami=Atrinik.WhoAmI()

msg = Atrinik.WhatIsMessage().strip().lower()
text = string.split(msg)

if msg == 'detect curse':
	if activator.PayAmount(200) == 1:
		whoami.SayTo(activator,"\nOk, i will cast a 'detect curse' for 2s on you.")
		activator.Write("You pay the money.", 0)
		whoami.CastAbility(activator,Atrinik.GetSpellNr("detect curse"), 1,0,"")
	else:
		whoami.SayTo(activator,"\nSorry, you have not enough money.")

elif msg == 'remove curse':
	if activator.PayAmount(300) == 1:
		whoami.SayTo(activator,"\nOk, i will cast a 'remove curse' for 3s on you.")
		activator.Write("You pay the money.", 0)
		whoami.CastAbility(activator,Atrinik.GetSpellNr("remove curse"), 1,0,"")
	else:
		whoami.SayTo(activator,"\nSorry, you have not enough money.")

elif msg == 'remove damnation':
	if activator.PayAmount(200) == 1:
		whoami.SayTo(activator,"\nOk, i will cast a 'remove damnation' for 30s on you.")
		activator.Write("You pay the money.", 0)
		whoami.CastAbility(activator,Atrinik.GetSpellNr("remove damnation"), 1,0,"")
	else:
		whoami.SayTo(activator,"\nSorry, you have not enough money.")

elif msg == 'detect magic':
	if activator.PayAmount(200) == 1:
		whoami.SayTo(activator,"\nOk, i will cast a 'detect magic' for 2s on you.")
		activator.Write("You pay the money.", 0)
		whoami.CastAbility(activator,Atrinik.GetSpellNr("detect magic"), 1,0,"")
	else:
		whoami.SayTo(activator,"\nSorry, you have not enough money.")

elif msg == 'remove depletion':
	if activator.PayAmount(200) == 1:
		whoami.SayTo(activator,"\nOk, i will cast a 'remove depletion' for 35s on you.")
		activator.Write("You pay the money.", 0)
		whoami.CastAbility(activator,Atrinik.GetSpellNr("remove depletion"), 1,0,"")
	else:
		whoami.SayTo(activator,"\nSorry, you have not enough money.")

elif msg == 'identify':
	object = activator.FindMarkedObject()
	if object == None:
		whoami.SayTo(activator,"\nYou must mark the item first")
	else:
		if activator.PayAmount(2000) == 1:
			whoami.SayTo(activator,"\nOk, i will cast a 'identify' for 20s over the "+object.name+".")
			activator.Write("You pay the money.", 0)
			whoami.IdentifyItem(activator, object, Atrinik.IDENTIFY_MARKED)
		else:
			whoami.SayTo(activator,"\nSorry, you have not enough money.")

elif msg == 'identify all':
	if activator.PayAmount(5000) == 1:
		whoami.SayTo(activator,"\nOk, i will cast a 'identify all' for 50s.")
		activator.Write("You pay the money.", 0)
		whoami.IdentifyItem(activator, None, Atrinik.IDENTIFY_ALL)
	else:
		whoami.SayTo(activator,"\nSorry, you have not enough money.")

elif msg == 'food':
	activator.food = 999
	whoami.SayTo(activator,'\nYour stomach is filled again.')

else:
	whoami.SayTo(activator,"\nWelcome to my shop. We have what you want!\nAs service I can cast 'detect curse' or 'detect magic' for 20s on your items. I can also 'identify' single items for only 40s or all for 2g.\nSay ^detect curse^ or ^magic^, ^remove curse^ or ^damnation^ or ^depletion^, ^identify^ or ^identify all^ if you need my service.") 

