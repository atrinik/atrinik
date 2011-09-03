## @file
## Implements the House Agency Manager.

from Atrinik import *
from House import *

activator = WhoIsActivator()
me = WhoAmI()
msg = WhatIsMessage().strip().lower()
text = msg.split()
house = House(activator, GetOptions())

def main():
	if msg == "hi" or msg == "hey" or msg == "hello":
		me.SayTo(activator, "\nWelcome to our house agency. We can sell you a luxury house anyone would be jealous of! Do you want to buy one or do you need more <a>information</a> first?")

		if not house.has_house():
			me.SayTo(activator, "\n<a>Buy Luxury House</a>", 1)

	elif msg == "information":
		me.SayTo(activator, "\nHouses are like apartments, with the only difference that they are much bigger, so they can store more items, and they are <a>buildable</a>.\nAlso to enter your house you will need to pay the daily <a>fees</a>.")

	elif msg == "buildable":
		me.SayTo(activator, "\nBuildable areas of your house allow you to customize the house to your liking with things like chests, altars, walls, and so on. Of course, this all costs a price, but construction and materials are available at the secret island called <green>Everlink</green> which can be accessed from your house.")

	elif msg == "fees":
		me.SayTo(activator, "\nThe {0} requires {1} as daily fee to operate, and comes with {2} days of prepaid fees. Fees are paid automatically whenever you enter the house (either from hand or bank acount). If you don't have enough money to pay, you won't be able to enter the house, but will be able to leave it and enter the <green>Everlink</green> area.\nNote that if you don't pay, for, say, 20 days, you won't have to pay for all those days that you did not use your house.".format(house.get(house.name), CostString(house.get(house.fee)), house.get(house.fees_prepaid)))

	else:
		if house.has_house():
			return

		elif msg == "buy luxury house":
			me.SayTo(activator, "\nCertainly! The {0} will cost you {1}. Are you sure that you want to buy this luxury house and that you understand the <a>fees</a> system?\n\nIf you are, say <green>I'm sure</green>.".format(house.get(house.name), CostString(house.get(house.cost))))

		elif msg == "i'm sure":
			if activator.PayAmount(house.get(house.cost)):
				activator.Write("You pay {}.".format(CostString(house.get(house.cost))), COLOR_WHITE)
				me.SayTo(activator, "\nThank you for your business! We hope you enjoy your brand new house.")
				house.add_house()
			else:
				me.SayTo(activator, "\nBut it seems that you do not have enough money!")

main()
