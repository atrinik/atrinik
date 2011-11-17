## @file
## Handles map-wide events in Auction Houses.

from Auction import item_buy

other = WhoIsOther()
event = GetEventNumber()

## Check whether the specified object is inside a player.
## @return True if the object is inside a player, False otherwise.
def is_in_player(obj):
	while obj.env:
		obj = obj.env

	return obj.type == Type.PLAYER

def main():
	# Do not handle the below events if the player is not standing in one
	# of the boxes.
	try:
		floor = activator.map.GetLayer(activator.x, activator.y, LAYER_FLOOR)[0]

		if not floor.f_unique:
			return
	except:
		return

	# Prevent drop-related commands in the boxes. Do not allow /take
	# command either, as an accidental '/take all' might not be very
	# pleasant.
	if event in (MEVENT_DROP, MEVENT_CMD_DROP, MEVENT_CMD_TAKE):
		activator.Write("You cannot do that here.", COLOR_RED)
		SetReturnValue(1)
	# Apply event, do not allow applying the items in the boxes.
	elif event == MEVENT_APPLY:
		SetReturnValue(-1)

		if not is_in_player(other):
			activator.Write("You cannot do that here.", COLOR_RED)
			SetReturnValue(OBJECT_METHOD_OK)
	# Picked up an item, try to pay for it.
	elif event == MEVENT_PICK:
		seller = other.ReadKey("auction_house_seller")

		if not seller or other.env:
			return

		SetReturnValue(1)
		activator.Write(item_buy(activator, other, GetEventParameters()[0], seller), COLOR_WHITE)
	elif event == MEVENT_EXAMINE:
		seller = other.ReadKey("auction_house_seller")

		if not seller:
			return

		activator.Write("<green>Auction House Information</green>:\nPrice: {} (each)\nSeller: {}".format(CostString(int(other.ReadKey("auction_house_value"))), seller), COLOR_WHITE)

main()
