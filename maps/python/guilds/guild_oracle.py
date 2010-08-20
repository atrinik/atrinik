## @file
## Guild Oracle provides guild administration features for the guild
## administrators.

from Atrinik import *
from Guild import Guild

activator = WhoIsActivator()
me = WhoAmI()
msg = WhatIsMessage().strip().lower()
text = msg.split()

def main():
	# Only allow administrators or Wizards to use this script.
	if not guild.member_is_admin(activator.name) and not activator.f_wiz:
		me.SayTo(activator, "\nYou are not the guild administrator.")
		return

	if msg == "hi" or msg == "hey" or msg == "hello":
		me.SayTo(activator, "\nHello {}. Do you want to manage ^members^, the ^guild^ itself or the ^ranks^?".format(activator.name))

		if activator.f_wiz:
			me.SayTo(activator, "Since you're the Wizard, you can also ^add^ members directly or change the guild's ^founder^.", 1)

	# Show member related commands.
	elif msg == "members":
		me.SayTo(activator, "\nI can show you the guild's list of ^applications^ or ^remove^ a member from the guild (removing will also close an application if any).\nOr would you rather ^give admin^ status to a member, or perhaps ^remove admin^ status from a fellow administrator?")

	# Show guild related commands.
	elif msg == "guild":
		closed = guild.guild_check(guild.guild_closed)
		me.SayTo(activator, "\nThe guild is currently {}. Would you like to ^{}^ it?".format(closed and "closed" or "open", closed and "open" or "close"))

	# Show rank related commands.
	elif msg == "ranks":
		me.SayTo(activator, "\nDo you want to see the ^ranks list^, create a ^new rank^, ^remove rank^, ^change rank^ setting or ^view rank^ setting?\nYou can also ^assign^ a member to one of the existing ranks and ^clear rank^ of a member, or see members ^without rank^.")

	# Show the list of ranks.
	elif msg == "ranks list":
		me.SayTo(activator, "\nList of ranks:\n")

		for rank in guild.ranks_get_sorted():
			me.SayTo(activator, guild.rank_string(rank), 1)

	# Show members without rank.
	elif msg == "without rank":
		l = list(filter(lambda name: not guild.member_get_rank(name), guild.get_members()))

		if l:
			me.SayTo(activator, "\nMembers without any rank:\n{}".format(", ".join(l)))
		else:
			me.SayTo(activator, "\nThere are no members without rank.")

	# Create a new rank.
	elif msg[:8] == "new rank":
		if not msg[9:]:
			me.SayTo(activator, "\nThe 'new rank' command can add a new rank to the list of ranks.\nExample: ~new rank Junior Member~")
		else:
			rank = guild.rank_sanitize(WhatIsMessage()[9:])

			if not rank:
				me.SayTo(activator, "\nInvalid rank name: it cannot contain special symbols/characters and the maximum length is {} characters.".format(guild.get(guild.rank_max_chars)))
				return

			if guild.rank_exists(rank):
				me.SayTo(activator, "\nThe rank '{}' already exists.".format(rank))
			elif guild.rank_add(rank):
				me.SayTo(activator, "\nSuccessfully created rank '{}'.".format(rank))
			else:
				me.SayTo(activator, "\nCould not add rank '{}'.".format(rank))

	# Assign member to a rank.
	elif msg[:6] == "assign":
		if len(text) < 3:
			me.SayTo(activator, "\nAssign what member to what rank?\nExample: ~assign Atrinik Junior Member~")
		else:
			name = text[1].capitalize()
			rank = WhatIsMessage()[7 + len(name) + 1:]

			if not guild.rank_exists(rank):
				me.SayTo(activator, "\nNo such rank '{}'.".format(rank))
			elif not guild.member_exists(name):
				me.SayTo(activator, "\nNo such member {}.".format(name))
			elif guild.member_set_rank(name, rank):
				me.SayTo(activator, "\nAdded {} to rank '{}'.".format(name, rank))
			else:
				me.SayTo(activator, "\nCould not add {} to rank '{}'".format(name, rank))

	# Remove a rank.
	elif msg[:11] == "remove rank":
		if not msg[12:]:
			me.SayTo(activator, "\nRemove what rank? Removing a rank will also clear rank of all members in that rank.\nExample: ~remove rank Junior Member~")
		else:
			rank = WhatIsMessage()[12:]

			if not guild.rank_exists(rank):
				me.SayTo(activator, "\nNo such rank '{}'.".format(rank))
			elif guild.rank_remove(rank):
				me.SayTo(activator, "\nRemoved rank '{}'.".format(rank))
			else:
				me.SayTo(activator, "\nRank '{}' could not be removed.".format(rank))

	# Clear member's rank.
	elif msg[:10] == "clear rank":
		if not msg[11:]:
			me.SayTo(activator, "\nClear rank for which member?\nExample: ~clear rank Atrinik~")
		else:
			name = msg[11:].capitalize()

			if not guild.member_exists(name):
				me.SayTo(activator, "\nNo such member {}.".format(name))
			elif guild.member_set_rank(name):
				me.SayTo(activator, "\n{} is no longer a member of any rank.".format(name))
			else:
				me.SayTo(activator, "\nCould not clear rank of {}.".format(name))

	# Change rank's setting.
	elif msg[:11] == "change rank":
		if len(text) < 5:
			me.SayTo(activator, "\nChange what? Examples:\nTo change rank's value limit (how much can be taken from the guild in copper, 0 means unlimited) to 1 gold and 50 silver:\n~change rank limit 15000 Junior Member~\nTo change when ranks get limit reset to 12 hours (default is 24 hours and the timer starts on the first pickup):\n~change rank time 12 Junior Member~")
		else:
			what = text[2]
			value = text[3]
			rank = WhatIsMessage()[12 + len(what) + len(value) + 2:]

			if not guild.rank_exists(rank):
				me.SayTo(activator, "\nNo such rank '{}'.".format(rank))
			elif what == "limit":
				if not value.isdigit() or int(value) < 0 or int(value) > guild.rank_value_max:
					me.SayTo(activator, "\nInvalid value to set, must be 0-{}.".format(guild.rank_value_max))
				elif guild.rank_set(rank, "value_limit", int(value)):
					me.SayTo(activator, "\nSuccessfully changed {} to {}.".format(what, CostString(int(value))))
				else:
					me.SayTo(activator, "\nCould not change {} to {}.".format(what, CostString(int(value))))
			elif what == "time":
				if not value.isdigit() or int(value) < guild.rank_reset_min or int(value) > guild.rank_reset_max:
					me.SayTo(activator, "\nInvalid value to set, must be {}-{}.".format(guild.rank_reset_min, guild.rank_reset_max))
				elif guild.rank_set(rank, "value_reset", int(value)):
					me.SayTo(activator, "\nSuccessfully changed {} to {} hour(s).".format(what, value))
				else:
					me.SayTo(activator, "\nCould not change {} to {} hour(s).".format(what, value))
			else:
				me.SayTo(activator, "\nInvalid setting, see ^change rank^ for possible settings.")

	# View rank's setting.
	elif msg[:9] == "view rank":
		if len(text) < 4:
			me.SayTo(activator, "\nView what? Examples:\nTo view rank's value limit (how much can be taken from the guild in copper, 0 means unlimited):\n~view rank limit Junior Member~\nTo view when ranks get limit reset:\n~view rank time Junior Member~")
		else:
			what = text[2]
			rank = WhatIsMessage()[10 + len(what) + 1:]

			if not guild.rank_exists(rank):
				me.SayTo(activator, "\nNo such rank '{}'.".format(rank))
			elif what == "limit":
				me.SayTo(activator, "\nThe setting '{}' is set to {} for rank '{}'.".format(what, CostString(guild.rank_get(rank, "value_limit")), rank))
			elif what == "time":
				me.SayTo(activator, "\nThe setting '{}' is set to {} hour(s) for rank '{}'.".format(what, guild.rank_get(rank, "value_reset"), rank))
			else:
				me.SayTo(activator, "\nInvalid setting, see ^view rank^ for possible settings.")

	# Close the guild.
	elif msg == "close":
		guild.guild_set(guild.guild_closed)
		me.SayTo(activator, "\nClosed the guild. No new membership applications will be accepted by the Guild Manager.")

	# Open the guild.
	elif msg == "open":
		guild.guild_unset(guild.guild_closed)
		me.SayTo(activator, "\nOpened the guild. New membership applications will be accepted by the Guild Manager.")

	# List members awaiting membership application.
	elif msg == "applications":
		me.SayTo(activator, "\nList of membership applications:")
		l = []

		for member in guild.get_members():
			if guild.member_approved(member):
				continue

			l.append(member)

		if l:
			l.sort()
			me.SayTo(activator, ", ".join(l), 1)
		else:
			me.SayTo(activator, "No membership applications.", 1)

	# Remove a member from the guild, or decline their membership application.
	elif msg[:6] == "remove":
		if not msg[7:]:
			me.SayTo(activator, "\nThe remove command can be used to remove a member from the guild, or decline their membership application.\nExample: ~remove Atrinik~")
		else:
			name = msg[7:].capitalize()

			if not guild.member_exists(name):
				me.SayTo(activator, "\nNo such member {}.".format(name))
			elif not activator.f_wiz and name == activator.name:
				me.SayTo(activator, "\nYou cannot remove yourself.")
			elif not activator.f_wiz and guild.is_founder(name):
				me.SayTo(activator, "\nYou cannot remove the guild founder.")
			else:
				if guild.member_remove(name):
					me.SayTo(activator, "\nSuccessfully removed {} from the guild.".format(name))
				else:
					me.SayTo(activator, "\nCould not remove {} from the guild.".format(name))

	# Approve a membership application.
	elif msg[:7] == "approve":
		if not msg[8:]:
			me.SayTo(activator, "\nThe approve command is used to approve a member that applied for the guild membership.\nExample: ~approve Atrinik~")
		else:
			name = msg[8:].capitalize()

			if not guild.member_exists(name):
				me.SayTo(activator, "\nNo such member {}.".format(name))
			elif guild.member_approved(name):
				me.SayTo(activator, "\nThis member has already been approved.")
			else:
				if guild.member_approve(name):
					me.SayTo(activator, "\nSuccessfully approved {} for full guild membership.".format(name))
				else:
					me.SayTo(activator, "\nSomething went wrong and {} could not be approved.".format(name))

	# Give administrator rights to a member.
	elif msg[:10] == "give admin":
		if not msg[11:]:
			me.SayTo(activator, "\nThe 'give admin' command is used to give administrator rights to a guild member.\nExample: ~give admin Atrinik~")
		else:
			name = msg[11:].capitalize()

			if not guild.member_exists(name):
				me.SayTo(activator, "\nNo such member {}.".format(name))
			elif guild.member_is_admin(name):
				me.SayTo(activator, "\nThis member is already an administrator.")
			else:
				if guild.member_admin_make(name):
					me.SayTo(activator, "\nSuccessfully made {} a guild administrator.".format(name))
				else:
					me.SayTo(activator, "\n{} cannot be made an administrator.".format(name))

	# Remove administrator rights from a member.
	elif msg[:12] == "remove admin":
		if not msg[13:]:
			me.SayTo(activator, "\nThe 'remove admin' command is used to take administrator rights from a guild member.\nExample: ~remove admin Atrinik~")
		else:
			name = msg[13:].capitalize()

			if not guild.member_exists(name):
				me.SayTo(activator, "\nNo such member {}.".format(name))
			elif not guild.member_is_admin(name):
				me.SayTo(activator, "\nThis member is not an administrator.")
			elif not activator.f_wiz and name == activator.name:
				me.SayTo(activator, "\nYou cannot take away administrator rights from yourself.")
			elif not activator.f_wiz and guild.is_founder(name):
				me.SayTo(activator, "\nYou cannot take administrator rights from the guild founder.")
			else:
				if guild.member_admin_remove(name):
					me.SayTo(activator, "\nSuccessfully removed administrator rights from {}.".format(name))
				else:
					me.SayTo(activator, "\n{} cannot have administrator rights taken away.".format(name))

	# Check DM commands.
	elif activator.f_wiz:
		# Directly add a member.
		if msg[:3] == "add":
			if not msg[4:]:
				me.SayTo(activator, "\nThe add command can be used by DMs to directly add a member to the guild.\nExample: ~add Atrinik~")
			else:
				name = msg[4:].capitalize()

				if not PlayerExists(name):
					me.SayTo(activator, "\nThe player {} does not exist.".format(name))
				elif guild.member_exists(name):
					me.SayTo(activator, "\nThe player {} is already a member of the guild.".format(name))
				else:
					guild.member_add(name)
					me.SayTo(activator, "\nSuccessfully added {} to the guild.".format(name))

		# Set founder of the guild.
		elif msg[:7] == "founder":
			if not msg[8:]:
				me.SayTo(activator, "\nThe founder command sets a founder of the guild.\nExample: ~founder Atrinik~")
			else:
				name = msg[8:].capitalize()

				if not guild.member_exists(name):
					me.SayTo(activator, "\nNo such member {}.".format(name))
				elif guild.set_founder(name):
					me.SayTo(activator, "\nSuccessfully made {} the guild founder.".format(name))
				else:
					me.SayTo(activator, "\nCould not make {} the guild founder.".format(name))

guild = Guild(GetOptions())
main()
