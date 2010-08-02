# Script to run once player exits the starting tutorial map.
# This will set the player's new save bed to church in Tutorial Island.

from Atrinik import *

pl = WhoIsActivator().Controller()

pl.savebed_map = "/shattered_islands/world_0303"
pl.bed_x = 6
pl.bed_y = 11
