## @file
## Generic bank script used for bank NPCs.

from Atrinik import *

activator = WhoIsActivator()
me = WhoAmI()

msg = WhatIsMessage().strip().lower()
text = msg.split()

## Player info tag of the bank object.
pinfo_tag = "BANK_GENERAL"

def main():
	if msg == "bank" or msg == "hello" or msg == "hi" or msg == "hey":
		me.SayTo(activator, "\nHello! I am {0}, the banker.\nWhat can I do for you? Do you need ^info^ about this bank?".format(me.name))

	# Give out information about the bank
	elif msg == 'info':
		me.SayTo(activator, "\nWith the keyword ^balance^ I will tell how much money you have stored here on your account.\nStore money with ^deposit^ <# gold, # silver...>.\nGet money with ^withdraw^ <# gold, # silver...>.")

	elif msg == 'balance':
		pinfo = activator.GetPlayerInfo(pinfo_tag)

		if pinfo == None or pinfo.value == 0:
			me.SayTo(activator, "\nYou have no money stored.")
		else:
			me.SayTo(activator, "\n{0}, your balance is {1}.".format(activator.name, activator.ShowCost(pinfo.value)))

	# Deposit money
	elif text[0] == 'deposit':
		pinfo = activator.GetPlayerInfo(pinfo_tag)

		if pinfo == None:
			pinfo = activator.CreatePlayerInfo(pinfo_tag)

		dpose = activator.Deposit(pinfo, msg)

		if dpose == 0:
			me.SayTo(activator, "\nYou don't have that much money.")
		elif dpose == 1:
			if pinfo.value != 0:
				me.SayTo(activator, "\n{0}, your new balance is {1}.".format(activator.name, activator.ShowCost(pinfo.value)))

	# Withdraw some money
	elif text[0] == 'withdraw':
		pinfo = activator.GetPlayerInfo(pinfo_tag)

		if pinfo == None or pinfo.value == 0:
			me.SayTo(activator, "\nYou have no money stored.")
		else:
			wdraw = activator.Withdraw(pinfo, msg)

			if wdraw == 0:
				me.SayTo(activator, "\nYou don't have that much money.")
			elif wdraw == 1:
				if pinfo.value == 0:
					me.SayTo(activator, "\nYou removed all your money.")
				else:
					me.SayTo(activator, "\n{0}, your new balance is {1}.".format(activator.name, activator.ShowCost(pinfo.value)))

main()
