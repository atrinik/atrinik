## @file
## Implements the /guildmembers command. It provides a way for guild
## members to look who else is in the guild, and whether they are online
## or not.

from Atrinik import *
from Guild import Guild

activator = WhoIsActivator()
guild = Guild(None)

## Check which guild the player is member of.
guildname = guild.is_in_guild(activator.name)

if guildname:
	msg = WhatIsMessage()
	all_members = msg == "all"

	guild.guildname = guildname
	founder = guild.get_founder()

	online_marker = all_members and FindPlayer(founder) and "~" or ""

	activator.Write("\n{0} of {1}:".format(all_members and "Members" or "Online members", guildname), COLOR_WHITE)
	activator.Write("Founder: " + online_marker + founder + online_marker, COLOR_WHITE)

	admins = []
	members = []

	for member in guild.guilddb[guild.guildname]["members"]:
		if not guild.is_approved(member):
			continue

		player = FindPlayer(member)

		# Do we only want online members, or all?
		if not all_members and not player:
			continue

		online_marker = all_members and player and "~" or ""

		if not guild.is_administrator(member):
			members.append(online_marker + member + online_marker)
		elif member != founder:
			admins.append(online_marker + member + online_marker)

	if admins:
		admins.sort()
		activator.Write("\nAdministrators:\n" + ", ".join(admins), COLOR_WHITE)

	if members:
		members.sort()
		activator.Write("\nMembers:\n" + ", ".join(members), COLOR_WHITE)

else:
	activator.Write("You are not member of any guild.", COLOR_RED)

# Close the guild database.
guild.guilddb.close()
