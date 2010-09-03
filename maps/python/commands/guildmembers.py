## @file
## Implements the /guildmembers command. It provides a way for guild
## members to look who else is in the guild, and whether they are online
## or not.

from Atrinik import *
from Guild import Guild

activator = WhoIsActivator()
guild = Guild(None)

def main():
	## Check which guild the player is member of.
	g = guild.pl_get_guild(activator.name)

	if not g or not g[2]:
		activator.Write("You are not member of any guild.", COLOR_RED)
		return

	msg = WhatIsMessage()
	# Do we want to show all members, or just online ones?
	all_members = msg == "all"

	# Simply switch the guild name we are managing.
	guild.set(g[0])
	# Get the guild founder.
	founder = guild.get_founder()

	# Online marker for the founder.
	online_marker = all_members and FindPlayer(founder) and "~" or ""

	activator.Write("\n{0} of {1}:".format(all_members and "Members" or "Online members", g[0]), COLOR_WHITE)
	activator.Write("Founder: " + online_marker + founder + online_marker, COLOR_WHITE)

	admins = []
	members = []

	for member in guild.get_members():
		if not guild.member_approved(member):
			continue

		player = FindPlayer(member)

		if player and player.Controller().dm_stealth and not (activator.f_wiz or activator.Controller().dm_stealth):
			player = None

		# Do we only want online members, or all?
		if not all_members and not player:
			continue

		# Online marker for the member.
		online_marker = all_members and player and "~" or ""

		# Regular member?
		if not guild.member_is_admin(member):
			members.append(online_marker + member + online_marker)
		# Otherwise an administrator, check that it's not the founder (which we showed above).
		elif member != founder:
			admins.append(online_marker + member + online_marker)

	if admins:
		admins.sort()
		activator.Write("\nAdministrators:\n" + ", ".join(admins), COLOR_WHITE)

	if members:
		members.sort()
		activator.Write("\nMembers:\n" + ", ".join(members), COLOR_WHITE)

main()
