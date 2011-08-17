## @file
## Jail manager NPC.

from Atrinik import *
from Jail import Jail

activator = WhoIsActivator()
me = WhoAmI()
msg = WhatIsMessage().strip().lower()
text = msg.split()

def main():
	if msg == "hi" or msg == "hey" or msg == "hello":
		me.SayTo(activator, "\nHello there, {0}. I am the jail manager, and I can assist you in hunting down criminals and putting them to this jail.\nSimply tell me which player to jail, and for how long, in the format of <green>jail</green> <green><name></green> <green><time></green>, where <yellow><name></yellow> is the player name to jail, and <yellow><time></yellow> is how long to jail the player for, in seconds. If 0, the player will be jailed for life.\nTo unjail a player, you can use <green>unjail</green> <green><name></green>.".format(activator.name))
	elif msg[:6] == "unjail":
		if len(text) < 2:
			me.SayTo(activator, "\nUse, for example, <a>unjail troublemakerxyxyx</a>.")
			return

		pl = FindPlayer(text[1])

		if not pl:
			me.SayTo(activator, "\nNo such player.")
			return

		j = Jail(me)
		force = j.get_jail_force(pl)

		if not force:
			me.SayTo(activator, "\n{0} is not in jail.".format(pl.name))
		else:
			force.Remove()
			me.SayTo(activator, "\n{0} has been released.".format(pl.name))
			pl.Write("You have been released early.", COLOR_GREEN)

	elif msg[:4] == "jail":
		if len(text) < 3 or not text[2].isdigit():
			me.SayTo(activator, "\nUse, for example, <a>jail troublemakerxyxyx 60</a>.")
			return

		pl = FindPlayer(text[1])

		if not pl:
			me.SayTo(activator, "\nNo such player.")
			return

		seconds = int(text[2])

		# Need to put a limit on it...
		if seconds > 30000:
			me.SayTo(activator, "\nMaximum amount of time you can jail a player for is: 30,000 seconds.")
			return

		j = Jail(me)

		if j.jail(pl, seconds):
			me.SayTo(activator, "\n{0} has been jailed successfully.".format(pl.name))
		else:
			me.SayTo(activator, "\n{0} is already in jail.".format(pl.name))

main()
