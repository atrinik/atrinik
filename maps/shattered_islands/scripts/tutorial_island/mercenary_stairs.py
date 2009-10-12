## @file
## Script to control stairs in Mercenary Guild and only allow entrance
## to the guild to fellow Mercenaries.

from Atrinik import *

## Activator object.
activator = WhoIsActivator()

## The Mercenary guild tag.
guild_tag = "Mercenary"

## Dictionary of maps to teleport the player to, depending whether we
## allowed them access or not.
maps = {
	"merc_guild":
	{
		"map": "/shattered_islands/tutorial_island/mercenary_guild",
		"x": 1,
		"y": 6,
	},
	"island":
	{
		"map": "/shattered_islands/world_0303",
		"x": 20,
		"y": 20,
	},
}

SetReturnValue(1)

## Guild force of the player.
guild_force = activator.GetGuildForce()

if guild_force.slaying != guild_tag:
	activator.Write("Only members of the Mercenaries can enter here.", 2)
	activator.Write("A strong magic guardian force pushes you back.", 3)
	activator.TeleportTo(maps["island"]["map"], maps["island"]["x"], maps["island"]["y"], 0)
else:
	activator.Write("You can enter.", 2)
	activator.Write("A magic guardian force moves you down the stairs.", 4)
	activator.TeleportTo(maps["merc_guild"]["map"], maps["merc_guild"]["x"], maps["merc_guild"]["y"], 0)
