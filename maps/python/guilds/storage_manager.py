## @file
## Script used for Guild Storage Manager NPCs, to explain how to use the
## storage and the rank limits.

from Interface import Interface
from Guild import Guild

inf = Interface(activator, me)
guild = Guild(GetOptions())

def main():
	if msg == "hello":
		inf.add_msg("Hello {}. Welcome to the guild's storage. Anything you drop in one of the rooms here will stay there until another guild member picks it up.".format(activator.name))
		inf.add_msg("Do you want to learn about the storage limits?")
		inf.add_link("Tell me more...", dest = "tellmore")

		if guild.member_is_admin(activator.name) or "[OP]" in player.cmd_permissions:
			inf.add_msg("As a guild administrator, you can drop any item into one of the non-pickable containers with custom name set to <green>rank access: Junior Member</green>, to only allow members of the Junior Member rank to open it.")
			inf.add_msg("Use <green>rank access: None</green> to reset it back so anyone can access it.")

	elif msg == "tellmore":
		inf.add_msg("Guilds often set up ranks to assign to members. Those ranks can control how much worth of items a member is allowed to take from the guild storage in a specified time limit. So be careful when dropping things in the rooms, because you may not be able to get them back right away!")
		inf.add_msg("If you have reached your limit but need to pick up an item, talk to a guild administrator or another fellow member.")

		remaining = guild.member_limit_remaining(activator.name)

		if remaining:
			from datetime import timedelta
			inf.add_msg("Your time limit resets in {}.".format(timedelta(seconds = remaining)))

main()
