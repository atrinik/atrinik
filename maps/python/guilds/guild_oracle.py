## @file
## Generic script used by NPCs to provide Guild Oracle (administration)
## features to the administrators.

from Atrinik import *
from Guild import Guild
import string

## Activator object.
activator = WhoIsActivator()
## Object who has the event object in their inventory.
me = WhoAmI()

## Get the guild name from the event options.
guildname = GetOptions()

## The guild we're managing.
guild = Guild(guildname)

msg = WhatIsMessage().strip().lower()
text = msg.split()

# Only allow administrators or DMs to use this script.
if not guild.is_administrator(activator.name) and not activator.f_wiz:
	me.SayTo(activator, "\nYou are not the guild administrator.")

# Greeting. DMs get a few extra commands.
elif text[0] == "hi" or text[0] == "hey" or text[0] == "hello":
	if activator.f_wiz:
		me.SayTo(activator, "\nList of commands:\nlist\nremove <member>\nmake_admin <member>\nremove_admin <member>\napplications\napprove <member>\nclose\nopen\nadd <member>\nfounder <member>")
	else:
		me.SayTo(activator, "\nList of commands:\nlist\nremove <member>\nmake_admin <member>\nremove_admin <member>\napplications\napprove <member>\nclose\nopen")

# List the guild members.
elif text[0] == "list":
	me.SayTo(activator, "\nList of members:")

	if guild.guilddb[guild.guildname]["founder"]:
		activator.Write("Founder: %s\n" % guild.guilddb[guild.guildname]["founder"], COLOR_NAVY)

	for member in guild.guilddb[guild.guildname]["members"]:
		if guild.guilddb[guild.guildname]["members"][member]["flags"] & guild.MEMBER_FLAG_REQUESTED:
			continue

		message = member

		if guild.guilddb[guild.guildname]["members"][member]["flags"] & guild.MEMBER_FLAG_ADMINISTRATOR:
			message += " (administrator)"

		activator.Write(message, COLOR_NAVY)

# List members awaiting membership application.
elif text[0] == "applications":
	me.SayTo(activator, "\nList of membership applications:")
	i = 0

	for member in guild.guilddb[guild.guildname]["members"]:
		if not guild.guilddb[guild.guildname]["members"][member]["flags"] & guild.MEMBER_FLAG_REQUESTED:
			continue

		activator.Write(member, COLOR_NAVY)
		i = 1

	if i == 0:
		activator.Write("No membership applications.", COLOR_NAVY)

# Remove a member from the guild, or decline their membership application.
elif text[0] == "remove":
	if not text[1]:
		me.SayTo(activator, "\nThe remove command can be used to remove a member from the guild, or decline their membership application.")
	else:
		member_name = text[1].capitalize()
		member = guild.member(member_name)

		if member == None:
			me.SayTo(activator, "\nNo such member %s." % member_name)
		elif not activator.f_wiz and member_name == activator.name:
			me.SayTo(activator, "\nYou cannot remove yourself.")
		elif not activator.f_wiz and guild.is_founder(member_name):
			me.SayTo(activator, "\nYou cannot remove the guild founder.")
		else:
			if guild.remove_member(member_name, me):
				me.SayTo(activator, "\nSuccessfully removed %s from the guild." % member_name)
			else:
				me.SayTo(activator, "\nCould not remove %s from the guild." % member_name)

# Approve a member's membership application.
elif text[0] == "approve":
	if not text[1]:
		me.SayTo(activator, "\nThe approve command is used to approve a member that applied for the guild membership.")
	else:
		member_name = text[1].capitalize()
		member = guild.member(member_name)

		if member == None:
			me.SayTo(activator, "\nNo such member %s." % member_name)
		elif not member["flags"] & guild.MEMBER_FLAG_REQUESTED:
			me.SayTo(activator, "\nThis member has already been approved.")
		else:
			if guild.approve_member(member_name):
				me.SayTo(activator, "\nSuccessfully approved %s for full guild membership." % member_name)
			else:
				me.SayTo(activator, "\nSomething went wrong and %s could not be approved." % member_name)

# Close the guild.
elif text[0] == "close":
	guild.close_guild();
	me.SayTo(activator, "Closed the guild. No new membership applications will be accepted by the Guild Manager.")

# Open the guild.
elif text[0] == "open":
	guild.open_guild();
	me.SayTo(activator, "Opened the guild. New membership applications will be accepted by the Guild Manager.")

# Remove administrator rights from a member.
elif text[0] == "remove_admin":
	if not text[1]:
		me.SayTo(activator, "\nThe remove_admin command is used to take administrator rights from a guild member.")
	else:
		member_name = text[1].capitalize()
		member = guild.member(member_name)

		if member == None:
			me.SayTo(activator, "\nNo such member %s." % member_name)
		elif not guild.is_administrator(member_name):
			me.SayTo(activator, "\nThis member is not an administrator.")
		elif not activator.f_wiz and member_name == activator.name:
			me.SayTo(activator, "\nYou cannot take away administrator rights from yourself.")
		elif not activator.f_wiz and guild.is_founder(member_name):
			me.SayTo(activator, "\nYou cannot take administrator rights from the guild founder.")
		else:
			if guild.remove_admin(member_name, me):
				me.SayTo(activator, "\nSuccessfully removed administrator rights for %s." % member_name)
			else:
				me.SayTo(activator, "\n%s cannot have administrator rights taken away." % member_name)

# Give administrator rights to a member.
elif text[0] == "make_admin":
	if not text[1]:
		me.SayTo(activator, "\nThe make_admin command is used to give administrator rights to a guild member.")
	else:
		member_name = text[1].capitalize()
		member = guild.member(member_name)

		if member == None:
			me.SayTo(activator, "\nNo such member %s." % member_name)
		elif guild.is_administrator(member_name):
			me.SayTo(activator, "\nThis member is already an administrator.")
		else:
			if guild.make_admin(member_name):
				me.SayTo(activator, "\nSuccessfully made %s the guild administrator." % member_name)
			else:
				me.SayTo(activator, "\n%s cannot be made an administrator." % member_name)

# DM command: Directly add a member.
elif text[0] == "add":
	if activator.f_wiz:
		if not text[1]:
			me.SayTo(activator, "\nThe add command can be used by DMs to directly add a member to the guild.")
		else:
			member_name = text[1].capitalize()
			member = guild.member(member_name)

			if not PlayerExists(member_name):
				me.SayTo(activator, "\nThe player %s does not exist." % member_name)
			elif member:
				me.SayTo(activator, "\nThe player %s is already a member of the guild." % member_name)
			else:
				guild.add_member(member_name, guild.MEMBER_FLAG_NONE)
				me.SayTo(activator, "\nSuccessfully added %s to the guild." % member_name)
	else:
		me.SayTo(activator, "\nYou don't have enough privileges for this command.")

# DM command: Set founder of the guild.
elif text[0] == "founder":
	if activator.f_wiz:
		if not text[1]:
			me.SayTo(activator, "\nThe founder command sets a founder of the guild.")
		else:
			member_name = text[1].capitalize()
			member = guild.member(member_name)

			if member == None:
				me.SayTo(activator, "\nNo such member %s." % member_name)
			else:
				if guild.make_founder(member_name):
					me.SayTo(activator, "\nSuccessfully made %s the guild founder." % member_name)
				else:
					me.SayTo(activator, "\nCould not make %s the guild founder." % member_name)
	else:
		me.SayTo(activator, "\nYou don't have enough privileges for this command.")

else:
	activator.Write("%s listens to you without answer. Perhaps you should try ^hi^, ^hey^ or ^hello^..." % me.name, 0)

# Close the database.
guild.guilddb.close()
