## @file
## Generic bank script used for bank NPCs.

from Atrinik import *
import string

## Activator object.
activator = WhoIsActivator()
## Object who has the event object in their inventory.
me = WhoAmI()

msg = WhatIsMessage().strip().lower()
text = string.split(msg)

## Player info tag of the bank object.
pinfo_tag = "BANK_GENERAL"

# Give out information about the bank
if msg == 'info':
	me.SayTo(activator, "\nWith the keyword ^balance^ I will tell how much money you have stored here on your account.\nStore money with ^deposit^ <# gold, # silver...>.\nGet money with ^withdraw^ <# gold, # silver...>.")

elif msg == 'balance':
	pinfo = activator.GetPlayerInfo(pinfo_tag)

	if pinfo == None or pinfo.value == 0:
		me.SayTo(activator, "%s, you have no money stored." % activator.name)
	else:
		me.SayTo(activator, "%s, your balance is %s." % (activator.name, activator.ShowCost(pinfo.value)))

# Deposit money
elif text[0] == 'deposit':
	pinfo = activator.GetPlayerInfo(pinfo_tag)

	if pinfo == None:
		pinfo = activator.CreatePlayerInfo(pinfo_tag)

	dpose = activator.Deposit(pinfo, msg)

	if dpose == 0:
		me.SayTo(activator, "%s, you don't have that much money." % activator.name)
	elif dpose == 1:
		if pinfo.value != 0:
			me.SayTo(activator, "%s, your new balance is %s." % (activator.name, activator.ShowCost(pinfo.value)))

# Withdraw some money
elif text[0] == 'withdraw':
	pinfo = activator.GetPlayerInfo(pinfo_tag)

	if pinfo == None or pinfo.value == 0:
		me.SayTo(activator, "%s, you have no money stored." % activator.name)
	else:
		wdraw = activator.Withdraw(pinfo, msg)

		if wdraw == 0:
			me.SayTo(activator, "%s, you don't have that much money." % activator.name)
		elif wdraw == 1:
			if pinfo.value == 0:
				me.SayTo(activator, "%s, you removed all your money." % activator.name)
			else:
				me.SayTo(activator, "%s, your new balance is %s." % (activator.name, activator.ShowCost(pinfo.value)))

# Greeting
elif msg == "bank" or msg == "hello" or msg == "hi" or msg == "hey":
	me.SayTo(activator, "\nHello! I am %s, the banker.\nWhat can I do for you? Do you need ^info^ about this bank?" % me.name)

else:
	activator.Write("%s, the banker seems not to notice you.\nYou should try ^hello^, ^hi^ or ^hey^..." % me.name, 0)
