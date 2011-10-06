## @file
## Implements the /guildmembers command. It provides a way for guild
## members to look who else is in the guild, and whether they are online
## or not.

from Guild import Guild

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

	activator.Write("\n{} of {}:".format("Members" if all_members else "Online members", g[0]), COLOR_WHITE)

	if all_members and FindPlayer(founder):
		name = "<green>{}</green>".format(founder)
	else:
		name = founder

	activator.Write("Founder: {}".format(name), COLOR_WHITE)

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

		if all_members and player:
			name = "<green>{}</green>".format(member)
		else:
			name = member

		# Regular member?
		if not guild.member_is_admin(member):
			members.append(name)
		# Otherwise an administrator, check that it's not the founder (which we showed above).
		elif member != founder:
			admins.append(name)

	if admins:
		admins.sort()
		activator.Write("\nAdministrators:\n{}".format(", ".join(admins)), COLOR_WHITE)

	if members:
		members.sort()
		activator.Write("\nMembers:\n{}".format(", ".join(members)), COLOR_WHITE)

main()
