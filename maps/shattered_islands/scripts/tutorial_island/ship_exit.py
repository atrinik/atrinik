# Script to run when player comes to Brynknot by the ship from Tutorial Island.
# This will set the player's save bed to church in Brynknot.

from Atrinik import *

# The activator
activator = WhoIsActivator()

# Player info tag name
pinfo_tag = "VISITED_BRYNKNOT"

# Search for the player info
pinfo = activator.GetPlayerInfo(pinfo_tag)

# If no player info, we haven't been to Brynknot before
if pinfo == None:
	# Set the save bed
	activator.SetSaveBed(ReadyMap("/shattered_islands/world_0312", 0), 3, 7)

	# Create the player info tag -- now we've been to Brynknot.
	activator.CreatePlayerInfo(pinfo_tag)
