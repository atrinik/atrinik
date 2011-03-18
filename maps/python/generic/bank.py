## @file
## Generic bank script used for bank NPCs.

from Atrinik import *

activator = WhoIsActivator()
me = WhoAmI()
msg = WhatIsMessage().strip().lower()
text = msg.split()

def main():
	if msg == "bank" or msg == "hello" or msg == "hi" or msg == "hey":
		me.SayTo(activator, "\nHello! I am {}, the banker.\nDo you want to <a>deposit all</a> or <a>withdraw all</a>, or should I <a>explain</a> how we work?".format(me.name))

	# Give out information about the bank
	elif msg == "explain":
		me.SayTo(activator, "\nStoring your money in a bank account is a wise idea indeed. Just ask for the <a>balance</a> to see what's in your account. You can also <a>deposit</a> money to your account or <a>withdraw</a> money from it.")

	elif msg == "balance":
		balance = activator.Controller().BankBalance()

		if balance == 0:
			me.SayTo(activator, "\nYou have no money stored in your bank account.\nWould you like to <a>deposit all</a> your money?")
		else:
			me.SayTo(activator, "\nYour balance is {}.".format(CostString(balance)))

	# Deposit money
	elif text[0] == "deposit":
		if len(text) == 1:
			me.SayTo(activator, "\nYou can store money in your account by saying <green>deposit</green> followed by the amount.\nFor example <green>deposit 1 mithril, 99 silver</green>\nYou can also <a>deposit all</a>.")
			return

		(ret, value) = activator.Controller().BankDeposit(msg)

		if ret == BANK_SYNTAX_ERROR:
			me.SayTo(activator, "\n<a>Deposit</a> what?\nUse <a>deposit all</a> or <green>deposit 40 gold, 20 silver</green>...")
		elif ret == BANK_DEPOSIT_COPPER:
			me.SayTo(activator, "\nYou don't have that many copper coins.")
		elif ret == BANK_DEPOSIT_SILVER:
			me.SayTo(activator, "\nYou don't have that many silver coins.")
		elif ret == BANK_DEPOSIT_GOLD:
			me.SayTo(activator, "\nYou don't have that many gold coins.")
		elif ret == BANK_DEPOSIT_MITHRIL:
			me.SayTo(activator, "\nYou don't have that many mithril coins.")
		elif ret == BANK_SUCCESS:
			balance = activator.Controller().BankBalance()

			if value:
				me.SayTo(activator, "\nYou deposit {}; your new balance is {}.".format(CostString(value), CostString(balance)))
			else:
				me.SayTo(activator, "\nYou don't have any money on hand.")

	# Withdraw some money
	elif text[0] == "withdraw":
		if len(text) == 1:
			me.SayTo(activator, "\nYou can get money from your account by saying <green>withdraw</green> followed by the amount.\nFor example <green>withdraw 2 gold, 40 copper</green>\nYou can also <a>withdraw all</a>.")
			return

		(ret, value) = activator.Controller().BankWithdraw(msg)

		if ret == BANK_SYNTAX_ERROR:
			me.SayTo(activator, "\n<a>Withdraw</a> what?\nUse <a>withdraw all</a> or <green>withdraw 30 gold, 20 silver</green>...")
		elif ret == BANK_WITHDRAW_HIGH:
			me.SayTo(activator, "\nYou can't withdraw that much at once.")
		elif ret == BANK_WITHDRAW_MISSING:
			me.SayTo(activator, "\nYou don't have that much money.")
		elif ret == BANK_WITHDRAW_OVERWEIGHT:
			me.SayTo(activator, "\nYou can't carry that much money.")
		elif ret == BANK_SUCCESS:
			balance = activator.Controller().BankBalance()

			if balance == 0:
				me.SayTo(activator, "\nYou removed your entire balance of {}.".format(CostString(value)))
			else:
				me.SayTo(activator, "\nYou withdraw {}; your new balance is {}.".format(CostString(value), CostString(balance)))

main()
