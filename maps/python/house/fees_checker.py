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
		# Get the cost.
		cost = house.get(house.fee)

		# Paid successfully?
		if activator.PayAmount(cost) == 1:
			activator.Write("You have paid {0} as your daily fee.".format(CostString(cost)), COLOR_GREEN)
			# Reset it to the current time.
			house.fees_reset()
			# Now add one more day.
			house.fees_add()
		# Failed to pay!
		else:
			activator.Write("Your prepaid fees have expired and you do not have {0} to pay for one more day.".format(CostString(cost)), COLOR_RED)
			(x, y) = house.get(house.fees_unpaid)
			activator.SetPosition(x, y)
