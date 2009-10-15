## @file
## This script manages the /guild command registered when Python plugin
## finished loading.
##
## The command provides a chat for guild members.

from Atrinik import *
from imp import load_source

## The Guild class.
Guild = load_source("Guild", CreatePathname("/python/Guild.py"))

## Activator object.
activator = WhoIsActivator()
## Get the message.
message = WhatIsMessage()

## The guild we're managing.
guild = Guild.Guild(None)

## Check which guild the player is member of.
guildname = guild.is_in_guild(activator.name)

if message and guildname != None:
	guild = Guild.Guild(guildname)

	for member in guild.guilddb[guild.guildname]["members"]:
		## Find the member, and if found, show him the guild message.
		player = FindPlayer(member)

		if player:
			player.Write("[%s] %s: %s" % (guild.guildname, member, message), COLOR_BLUE | NDI_PLAYER)
elif guildname == None:
	activator.Write("You are not member of any guild.", COLOR_RED)
else:
	activator.Write("You must provide a message to send to other guild members.", COLOR_RED)

# Close the guild database.
guild.guilddb.close()
