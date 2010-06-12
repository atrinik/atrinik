## @file
## Script used to implement stairs in Brynknot mercenary guild.
## The stairs only allow entrance to the Mercenary guild if the activator
## if member of the Mercenary guild.

from Atrinik import *

## Activator object.
activator = WhoIsActivator()

## Guild tag to look for in activator.
guild_tag = "Mercenary"

## Dictionary of maps where to teleport the activator depending whether
## they are member of the guild or not.
maps = {
	"merc_guild":
	{
		"map": "/shattered_islands/strakewood_island/brynknot/sewers_0102",
		"x": 11,
		"y": 14,
	},
	"island":
	{
		"map": "/shattered_islands/world_0112",
		"x": 19,
		"y": 11,
	},
}

SetReturnValue(1)

## Get the guild force from activator's inventory.
guild_force = activator.GetGuildForce()

if guild_force.slaying != guild_tag:
	activator.Write("Only members of the Mercenaries can enter here.", 2)
	activator.Write("A strong magic guardian force pushes you back.", 3)
	activator.TeleportTo(maps["island"]["map"], maps["island"]["x"], maps["island"]["y"], 0)
else:
	activator.Write("You can enter.", 2)
	activator.Write("A magic guardian force moves you down the stairs.", 4)
	activator.TeleportTo(maps["merc_guild"]["map"], maps["merc_guild"]["x"], maps["merc_guild"]["y"], 0)
