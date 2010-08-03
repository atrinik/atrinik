## @file
## Generic script used by NPCs in guilds to provide functions like
## requesting guild membership, actually entering the guild, leaving the
## guild, etc.

from Atrinik import *
from Guild import Guild

## Activator object.
activator = WhoIsActivator()
## Object who has the event object in their inventory.
me = WhoAmI()

## Get the event options.
event_options = GetOptions().split("|")

## Get the guild name.
guildname = event_options[0]

msg = WhatIsMessage().strip().lower()
text = msg.split()

## The guild we're managing.
guild = Guild(guildname)

def main():
	# Greeting section.
	if text[0] == "hi" or text[0] == "hey" or text[0] == "hello":
		## Get the member of the guild.
		member = guild.member(activator.name)

		if member == None:
			if not guild.is_closed():
				me.SayTo(activator, "\nWelcome to the {0}, {1}!\nDo you want to ^join^ the guild by requesting a guild membership?".format(guild.guildname, activator.name))
			else:
				me.SayTo(activator, "\nWelcome {0}.\nUnfortunately, this guild does not accept new members right now.".format(activator.name))
		elif member["flags"] & guild.MEMBER_FLAG_REQUESTED:
			me.SayTo(activator, "\nYour membership application request has not been decided yet. If you want to cancel it, say ^leave^.")
		else:
			me.SayTo(activator, "\nWelcome back {0}. It's good to see you again.\nIf you want to enter the guild, say ^enter^. If you no longer wish to be a member of this guild, say ~leave~.".format(activator.name))

	# Enter the guild.
	elif msg == "enter":
		## Is the activator a legitimate guild member?
		is_member = guild.is_member_of(activator.name)

		if is_member == True:
			activator.SetPosition(int(event_options[1]), int(event_options[2]))
		else:
			me.SayTo(activator, "\nYou cannot enter this guild.")

	# Leave the guild.
	elif msg == "leave":
		member = guild.member(activator.name)

		if member == None:
			me.SayTo(activator, "\nYou are not member of this guild, so you cannot leave it.")
		else:
			guild.remove_member(activator.name, me)

			if member["flags"] & guild.MEMBER_FLAG_REQUESTED:
				me.SayTo(activator, "\nYour membership application requested has been canceled.")
			else:
				me.SayTo(activator, "\nYou have left the guild.")

	# Request a guild membership.
	elif msg == "join":
		if not guild.is_closed():
			member = guild.member(activator.name)

			if member == None:
				if guild.is_in_guild(activator.name):
					me.SayTo(activator, "\nYou are already a member of another guild. In order to join a new guild, you must first leave your old one.")
				else:
					guild.add_member(activator.name, guild.MEMBER_FLAG_REQUESTED)
					me.SayTo(activator, "\nYou have been accepted to the guild. Before you are a fully qualified member however, you must wait for an approval from a guild administrator.")
			else:
				if member["flags"] & guild.MEMBER_FLAG_REQUESTED:
					me.SayTo(activator, "\nYou have already requested membership to the guild.")
				else:
					me.SayTo(activator, "\nYou are already a member of this guild.")
		else:
			me.SayTo(activator, "\nThis guild is closed. No new applications are accepted.")

try:
	main()
finally:
	guild.guilddb.close()
