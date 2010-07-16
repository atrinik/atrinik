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
		me.SayTo(activator, "\nHello! I am {0}, the banker.\nDo you want to ^deposit all^ or ^withdraw all^, or should I ^explain^ how we work?".format(me.name))

	# Give out information about the bank
	elif msg == "explain":
		me.SayTo(activator, "\nStoring your money in a bank account is a wise idea indeed. Just ask for the ^balance^ to see what's in your account. You can also ^deposit^ money to your account or ^withdraw^ money from it.")

	elif msg == "balance":
		pinfo = activator.GetPlayerInfo(pinfo_tag)

		if pinfo == None or pinfo.value == 0:
			me.SayTo(activator, "\nYou have no money stored in your bank account.\nWould you like to ^deposit all^ your money?")
		else:
			me.SayTo(activator, "\nYour balance is {0}.".format(activator.ShowCost(pinfo.value)))

	# Deposit money
	elif text[0] == "deposit":
		if len(text) == 1:
			me.SayTo(activator, "\nYou can store money in your account by saying ~deposit~ followed by the amount.\nFor example ~deposit 1 mithril, 99 silver~\nYou can also ^deposit all^.")
			return

		pinfo = activator.GetPlayerInfo(pinfo_tag)

		if not pinfo:
			pinfo = activator.CreatePlayerInfo(pinfo_tag)

		ret = activator.Deposit(pinfo, msg)

		if ret == BANK_SYNTAX_ERROR:
			me.SayTo(activator, "\nDeposit what?\nUse 'deposit all' or 'deposit 40 gold, 20 silver...'")
		elif ret == BANK_DEPOSIT_COPPER:
			me.SayTo(activator, "\nYou don't have that many copper coins.")
		elif ret == BANK_DEPOSIT_SILVER:
			me.SayTo(activator, "\nYou don't have that many silver coins.")
		elif ret == BANK_DEPOSIT_GOLD:
			me.SayTo(activator, "\nYou don't have that many gold coins.")
		elif ret == BANK_DEPOSIT_MITHRIL:
			me.SayTo(activator, "\nYou don't have that many mithril coins.")
		elif ret == BANK_SUCCESS:
			if pinfo.value != 0:
				me.SayTo(activator, "\nYour new balance is {0}.".format(activator.ShowCost(pinfo.value)))
			else:
				me.SayTo(activator, "\nYou don't have any money on hand.")

	# Withdraw some money
	elif text[0] == "withdraw":
		if len(text) == 1:
			me.SayTo(activator, "\nYou can get money from your account saying by ~withdraw~ followed by the amount.\nFor example ~withdraw 2 gold, 40 copper~\nYou can also ^withdraw all^.")
			return

		pinfo = activator.GetPlayerInfo(pinfo_tag)

		if not pinfo or pinfo.value == 0:
			me.SayTo(activator, "\nYou have no money stored.")
			return

		ret = activator.Withdraw(pinfo, msg)

		if ret == BANK_SYNTAX_ERROR:
			me.SayTo(activator, "\nWithdraw what?\nUse 'withdraw all' or 'withdraw 30 gold, 20 silver...'")
		elif ret == BANK_WITHDRAW_HIGH:
			me.SayTo(activator, "\nYou can't withdraw that much at once.")
		elif ret == BANK_WITHDRAW_MISSING:
			me.SayTo(activator, "\nYou don't have that much money.")
		elif ret == BANK_SUCCESS:
			if pinfo.value == 0:
				me.SayTo(activator, "\nYou removed all your money.")
			else:
				me.SayTo(activator, "\nYour new balance is {0}.".format(activator.ShowCost(pinfo.value)))

main()
