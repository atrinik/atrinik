## @file
## This script manages the /guild command registered when Python plugin
## finished loading.
##
## The command provides a chat for guild members.

from Atrinik import *
from Guild import Guild

activator = WhoIsActivator()
guild = Guild(None)

def main():
	# Check which guild the player is member of.
	guildname = guild.is_in_guild(activator.name)

	if not guildname:
		activator.Write("You are not member of any guild.", COLOR_RED)
		return

	message = WhatIsMessage()

	# Do we have a message? Then clean it up.
	if message:
		message = CleanupChatString(message)

	if not message:
		activator.Write("You must provide a message to send to other guild members.", COLOR_RED)
		return

	# Simply switch the guild name we are managing.
	guild.guildname = guildname
	LOG(llevInfo, "CLOG GUILD: {0} [{1}] >{2}<\n".format(activator.name, guildname, message))

	for member in guild.guilddb[guild.guildname]["members"]:
		if not guild.is_approved(member):
			continue

		# Find the member, and if found, show him the guild message.
		player = FindPlayer(member)

		if player:
			player.Write("[{0}] {1}: {2}".format(guild.guildname, activator.name, message), COLOR_BLUE | NDI_PLAYER)

try:
	main()
finally:
	guild.guilddb.close()
