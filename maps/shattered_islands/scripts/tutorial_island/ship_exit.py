## @file
## Script to run when player comes to Brynknot by the ship from Tutorial
## Island.
##
## This will set the player's save bed to church in Brynknot, if the
## player's current save bed is on Tutorial Island.

from Atrinik import *

## The activator.
activator = WhoIsActivator()

if activator.GetSaveBed()["map"] == "/shattered_islands/world_0303":
	activator.SetSaveBed(ReadyMap("/shattered_islands/world_0312", 0), 3, 7)
