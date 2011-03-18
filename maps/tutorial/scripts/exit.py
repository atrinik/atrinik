## @file
## Script used for various exits on Tutorial Island to update the
## player's savebed.

from Atrinik import *

me = WhoAmI()
activator = WhoIsActivator()
pl = activator.Controller()
options = GetOptions()

# Entering the cave - if the player logs out on the cave map, the map
# disappears (swapped out and reset because it won't be saved) and player
# logs back in, they will be sent back to the ladder that will let them
# enter the cave.
if options == "cave-enter":
	pl.savebed_map = activator.map.path
	pl.bed_x = activator.x
	pl.bed_y = activator.y
# Exiting either the cave or the tutorial island so update the save bed -
# don't want players to have any access back.
elif options == "cave-exit" or options == "island-exit":
	# Exiting cave, remove no longer needed weapons master force.
	if options == "cave-exit":
		obj = activator.FindObject(name = "tutorial_weapons_master")

		if obj:
			obj.Remove()

	pl.savebed_map = me.slaying
	pl.bed_x = me.hp
	pl.bed_y = me.sp
