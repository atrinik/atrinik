## @file
## Script to run when player comes to Brynknot by the ship from Tutorial
## Island.
##
## This will set the player's save bed to church in Brynknot, if the
## player's current save bed is on Tutorial Island.

from Atrinik import *

pl = WhoIsActivator().Controller()

if pl.savebed_map == "/shattered_islands/world_0303":
	pl.savebed_map = "/shattered_islands/world_0312"
	pl.bed_x = 3
	pl.bed_y = 7
