## @file
## Script for guild manager NPCs to manage applications, allow entering the
## guild, etc.

from Atrinik import *
from Guild import Guild

activator = WhoIsActivator()
me = WhoAmI()
msg = WhatIsMessage().strip().lower()

def main():
	# Greeting section.
	if msg == "hi" or msg == "hey" or msg == "hello":
		if not guild.member_exists(activator.name):
			if not guild.guild_check(guild.guild_closed):
				me.SayTo(activator, "\nWelcome to the {}, {}!\nDo you want to ^join^ the guild by requesting a guild membership?".format(guild.get_name(), activator.name))
			else:
				me.SayTo(activator, "\nWelcome {}.\nUnfortunately, this guild does not accept new members right now.".format(activator.name))
		elif not guild.member_approved(activator.name):
			me.SayTo(activator, "\nYour membership application request has not been decided yet. If you want to cancel it, say ~leave~.")
		else:
			me.SayTo(activator, "\nWelcome back {}. It's good to see you again.\nIf you want to enter the guild, say ^enter^. If you no longer wish to be a member of this guild, say ~leave~.".format(activator.name))

	# Enter the guild.
	elif msg == "enter":
		# Is the activator a legitimate guild member?
		if guild.member_approved(activator.name) or activator.f_wiz:
			(m, x, y) = guild.get(guild.enter_pos)
			activator.TeleportTo(m, x, y)
		else:
			me.SayTo(activator, "\nYou cannot enter this guild.")

	# Leave the guild.
	elif msg == "leave":
		if not guild.member_exists(activator.name):
			me.SayTo(activator, "\nYou are not member of this guild, so you cannot leave it.")
		else:
			approved = guild.member_approved(activator.name)
			guild.member_remove(activator.name)

			if not approved:
				me.SayTo(activator, "\nYour membership application request has been canceled.")
			else:
				me.SayTo(activator, "\nYou have left the guild.")

	# Request a guild membership.
	elif msg == "join":
		if not guild.guild_check(guild.guild_closed):
			if not guild.member_exists(activator.name):
				if guild.pl_get_guild(activator.name):
					me.SayTo(activator, "\nYou are already a member of another guild. In order to join a new guild, you must first leave your old one.")
				else:
					guild.member_add(activator.name, guild.member_requested)
					me.SayTo(activator, "\nYou have been accepted to the guild. Before you are a fully qualified member however, you must wait for an approval from a guild administrator.")
			else:
				if not guild.member_approved(activator.name):
					me.SayTo(activator, "\nYou have already requested membership to the guild.")
				else:
					me.SayTo(activator, "\nYou are already a member of this guild.")
		else:
			me.SayTo(activator, "\nThis guild is closed. No new applications are accepted.")

guild = Guild(GetOptions())
main()
