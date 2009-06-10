import Atrinik
import string

activator=Atrinik.WhoIsActivator()
whoami=Atrinik.WhoAmI()

msg = Atrinik.WhatIsMessage().strip().lower()
text = string.split(msg)

if msg == 'detect curse':
	if activator.PayAmount(50) == 1:
		whoami.SayTo(activator,"\nOk, I will cast a 'detect curse' for 50c on you.")
		activator.Write( "You pay the money.", 0)
		whoami.CastAbility(activator,Atrinik.GetSpellNr("detect curse"), 1,0,"")
	else:
		whoami.SayTo(activator,"\nSorry, you do not have enough money.")

elif msg == 'detect magic':
	if activator.PayAmount(50) == 1:
		whoami.SayTo(activator,"\nOk, I will cast a 'detect magic' for 50c on you.")
		activator.Write( "You pay the money.", 0)
		whoami.CastAbility(activator,Atrinik.GetSpellNr("detect magic"), 1,0,"")
	else:
		whoami.SayTo(activator,"\nSorry, you do not have enough money.")


elif msg == 'identify':
	object = activator.FindMarkedObject()
	if object == None:
		whoami.SayTo(activator,"\nYou must mark the item first")
	else:
		if activator.PayAmount(1) == 1:
			whoami.SayTo(activator,"\nOk, I will cast a 'identify' for 150c over the "+object.name+".")
			activator.Write( "You pay the money.", 0)
			whoami.IdentifyItem(activator, object, Atrinik.IDENTIFY_MARKED)
		else:
			whoami.SayTo(activator,"\nSorry, you do not have enough money.")

elif msg == 'identify all':
	if activator.PayAmount(500) == 1:
		whoami.SayTo(activator,"\nOk, I will cast a 'identify all' for 5s.")
		activator.Write( "You pay the money.", 0)
		whoami.IdentifyItem(activator, None, Atrinik.IDENTIFY_ALL)
	else:
		whoami.SayTo(activator,"\nSorry, you have not enough money.")

elif msg == 'hello' or msg == 'hi' or msg == 'hey':
	whoami.SayTo(activator,"\nWelcome to my shop. We have what you want!\nAs service I can cast 'detect curse' or 'detect magic' for 50c on your items. I can also 'identify' single items for only 150c or all for 5s.\nSay ^detect curse^, ^detect magic^, ^identify^ or ^identify all^ if you need my services."); 
