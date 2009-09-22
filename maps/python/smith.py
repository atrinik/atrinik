import Atrinik
import string

activator = Atrinik.WhoIsActivator()
me = Atrinik.WhoAmI()

msg = Atrinik.WhatIsMessage().strip().lower()
text = string.split(msg)

if msg == "detect curse":
	if activator.PayAmount(50) == 1:
		me.SayTo(activator, "\nOk, I will detect curse on your items for 50 copper coins.")
		activator.Write("You pay the money.", 0)
		me.CastAbility(activator, Atrinik.GetSpellNr("detect curse"), 1, 0, "")
	else:
		me.SayTo(activator, "\nSorry, you do not have enough money.")

elif msg == "detect magic":
	if activator.PayAmount(50) == 1:
		me.SayTo(activator, "\nOk, I will detect magic on your items for 50 copper coins.")
		activator.Write("You pay the money.", 0)
		me.CastAbility(activator, Atrinik.GetSpellNr("detect magic"), 1, 0, "")
	else:
		me.SayTo(activator, "\nSorry, you do not have enough money.")

elif msg == "identify":
	marked_object = activator.FindMarkedObject()

	if marked_object == None:
		me.SayTo(activator, "\nYou must mark an item first.")
	else:
		if activator.PayAmount(1) == 1:
			me.SayTo(activator, "\nOk, I will identify the %s for 150 copper coins." % marked_object.name)
			activator.Write("You pay the money.", 0)
			me.IdentifyItem(activator, marked_object, Atrinik.IDENTIFY_MARKED)
		else:
			me.SayTo(activator, "\nSorry, you do not have enough money.")

elif msg == "identify all":
	if activator.PayAmount(500) == 1:
		me.SayTo(activator, "\nOk, I will identify all your items for 5 silver coins.")
		activator.Write("You pay the money.", 0)
		me.IdentifyItem(activator, None, Atrinik.IDENTIFY_ALL)
	else:
		me.SayTo(activator, "\nSorry, you do not have enough money.")

elif msg == "hello" or msg == "hi" or msg == "hey":
	me.SayTo(activator, "\nWelcome to my shop. We have what you want!\nI can detect curse or magic on your items for 50 copper coins. I can also identify a single marked item for only 150 copper coins or all your items for 5 silver coins.\nSay ^detect curse^, ^detect magic^, ^identify^ or ^identify all^ if you need my services.");
