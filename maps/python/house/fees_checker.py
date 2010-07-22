## @file
## Script for invisible exits in houses that will check whether the
## player has paid their fees, and if not, will try to pay with the money
## on them, if that doesn't work, they'll be kicked out of the house.

from Atrinik import *
from House import *

activator = WhoIsActivator()
# We don't want the exits to actually work no matter what.
SetReturnValue(1)

if activator.type == TYPE_PLAYER:
	house = House(activator, GetOptions())

	# Have the fees expires?
	if house.fees_expired():
		days = house.fees_days()
		# Get the cost.
		cost = house.fees_cost_days(days)

		# Paid successfully?
		if activator.PayAmount(cost) == 1:
			activator.Write("You have paid {0} for {1} day(s) of fees.".format(activator.ShowCost(cost), days), COLOR_GREEN)
			house.fees_pay(days)
		# Failed to pay!
		else:
			activator.Write("Your prepaid fees have expired and you do not have {0} to pay for {1} day(s).".format(activator.ShowCost(cost), days), COLOR_RED)
			(x, y) = house.get(house.fees_unpaid)
			activator.SetPosition(x, y)
