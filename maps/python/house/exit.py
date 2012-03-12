## @file
## Script for exits related to the luxury houses.

from House import House

options = GetOptions().split(",")
house = House(activator, options[0])

def main():
	# Either going to Everlink or coming from Everlink.
	if options[0] == "everlink":
		# Coming from Everlink to house.
		if len(options) == 1:
			# No last house? Teleport them to the emergency map to be safe.
			if not house.get_last_house():
				pos = ("/emergency", 0, 0, 0)
			# Otherwise get the location they came from in their house.
			else:
				house.set_house(house.get_last_house())
				pos = house.get(house.portal_from_everlink)
		# Going to Everlink, just map coords of Everlink.
		else:
			pos = house.get_everlink(house.everlink_pos)
			house.set_last_house(options[1])
	# Getting out of their house, get the location where they should appear.
	elif options[1] == "out":
		pos = house.get(house.portal_out)
	# Otherwise entering their house.
	else:
		# They have a house here, teleport them to it.
		if house.has_house():
			pos = house.get(House.portal_house)
		# No house, teleport them out.
		else:
			pos = (activator.map.path, int(options[1]), int(options[2]), 0)
			pl.DrawInfo("You don't own a house here!", COLOR_RED)

	# Actually do the teleport.
	activator.TeleportTo(pos[0], pos[1], pos[2], pos[3])

main()
SetReturnValue(1)
