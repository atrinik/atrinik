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
	g = guild.pl_get_guild(activator.name)

	if not g or not g[2]:
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
	guild.set(g[0])
	guildname = guild.get_name()
	LOG(llevInfo, "CLOG GUILD: {0} [{1}] >{2}<\n".format(activator.name, guildname, message))

	for member in guild.get_members():
		if not guild.member_approved(member):
			continue

		# Find the member, and if found, show him the guild message.
		player = FindPlayer(member)

		if player:
			player.Write("[{0}] {1}: {2}".format(guildname, activator.name, message), COLOR_BLUE | NDI_PLAYER)

main()
