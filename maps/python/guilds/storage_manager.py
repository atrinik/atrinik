## @file
## Script used for Guild Storage Manager NPCs, to explain how to use the
## storage and the rank limits.

from Atrinik import *
from Guild import Guild
from datetime import timedelta

activator = WhoIsActivator()
me = WhoAmI()
msg = WhatIsMessage().strip().lower()

def main():
	if msg == "hi" or msg == "hey" or msg == "hello":
		me.SayTo(activator, "\nHello {}. Welcome to the guild's storage. Anything you drop in one of the rooms here will stay there until another guild member picks it up.\nDo you want to learn about the ^limits^?".format(activator.name))

		if guild.member_is_admin(activator.name) or activator.f_wiz:
			me.SayTo(activator, "\nAs a guild administrator, you can drop any item into one of the non-pickable containers with custom name set to ~rank access: Junior Member~, to only allow members of the Junior Member rank to open it.\nUse ~rank access: None~ to reset it back so anyone can access it.", 1)

	# Explain storage limits.
	elif msg == "limits":
		me.SayTo(activator, "\nGuilds often set up ranks to assign to members. Those ranks can control how much worth of items a member is allowed to take from the guild storage in a specified time limit. So be careful when dropping things in the rooms, because you may not be able to get them back right away!\nIf you have reached your limit but need to pick up an item, talk to a guild administrator or another fellow member.")
		remaining = guild.member_limit_remaining(activator.name)

		if remaining:
			me.SayTo(activator, "\nYour time limit resets in {0.".format(timedelta(seconds = remaining)), 1)

guild = Guild(GetOptions())

try:
	main()
finally:
	guild.exit()
