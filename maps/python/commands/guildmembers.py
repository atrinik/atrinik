## @file
## Implements the /guildmembers command. It provides a way for guild
## members to look who else is in the guild, and whether they are online
## or not.

from Atrinik import *
from Guild import Guild

## Activator object.
activator = WhoIsActivator()

## The guild we're managing.
guild = Guild(None)

## Check which guild the player is member of.
guildname = guild.is_in_guild(activator.name)

if guildname:
	only_online = WhatIsMessage() == "online"
	guild = Guild(guildname)

	activator.Write("\n%s of %s guild:" % (only_online and "Online members" or "Members", guildname), COLOR_WHITE)

	for member in guild.guilddb[guild.guildname]["members"]:
		player = FindPlayer(member)

		if not only_online or player:
			activator.Write("%(online)s%(name)s%(online)s%(founder)s%(admin)s" % {"online": (player and not only_online) and "~" or "", "name": member, "founder": guild.is_founder(member) and " (founder)" or "", "admin": guild.is_administrator(member) and " (administrator)" or ""}, COLOR_WHITE)
else:
	activator.Write("You are not member of any guild.", COLOR_RED)

# Close the guild database.
guild.guilddb.close()