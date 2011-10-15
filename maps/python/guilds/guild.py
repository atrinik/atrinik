## @file
## Script for guild manager NPCs to manage applications, allow entering the
## guild, etc.

from Interface import Interface
from Guild import Guild

inf = Interface(activator, me)
guild = Guild(GetOptions())

def main():
	if msg == "hello":
		if not guild.member_exists(activator.name):
			if not guild.guild_check(guild.guild_closed):
				inf.add_msg("Welcome to the {}, {}!".format(guild.get_name(), activator.name))
				inf.add_msg("Do you want to join the guild by requesting a guild membership?")
				inf.add_link("Sure...", dest = "dorequest")
			else:
				inf.add_msg("Welcome {}.".format(activator.name))
				inf.add_msg("Unfortunately, this guild does not accept new members right now.")
		elif not guild.member_approved(activator.name):
			inf.add_msg("Your membership application request has not been decided yet. If you want to cancel it, say leave. Do you wish to cancel it?")
			inf.add_link("Yes, cancel my application.", dest = "doleave")
		else:
			inf.add_msg("Welcome back {}. It's good to see you again.".format(activator.name))
			inf.add_msg("Would you like to enter the guild? Or do you want to leave the guild?")
			inf.add_link("I'd like to enter the guild.", dest = "doenter")
			inf.add_link("I no longer wish to be a member of this guild.", dest = "doleave")

	# Enter the guild.
	elif msg == "doenter":
		# Is the activator a legitimate guild member?
		if guild.member_approved(activator.name) or activator.f_wiz:
			(m, x, y) = guild.get(guild.enter_pos)
			activator.TeleportTo(m, x, y)
			inf.dialog_close()

	# Leave the guild.
	elif msg == "doleave":
		if not guild.member_exists(activator.name):
			return

		approved = guild.member_approved(activator.name)
		guild.member_remove(activator.name)

		if not approved:
			inf.add_msg("Your membership application request has been canceled.")
		else:
			inf.add_msg("You have left the guild.")

	elif msg == "dorequest":
		if guild.guild_check(guild.guild_closed) or guild.member_exists(activator.name):
			return

		if guild.pl_get_guild(activator.name):
			inf.add_msg("You are already a member of another guild. In order to join a new guild, you must first leave your old one.")
		else:
			guild.member_add(activator.name, guild.member_requested)
			inf.add_msg("You have been accepted to the guild. Before you are a fully qualified member however, you must wait for an approval from a guild administrator.")

main()
inf.finish()
