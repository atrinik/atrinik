## @file
## Simple guild greeter, handles showing player's guild rank and telling
## about all the ranks available.

from Atrinik import *
from Guild import Guild

activator = WhoIsActivator()
me = WhoAmI()
msg = WhatIsMessage().strip().lower()

def main():
	if msg == "hi" or msg == "hey" or msg == "hello":
		me.SayTo(activator, "\nWelcome to the guild, {}. Would you like to see the ^ranks^ list?".format(activator.name))
		rank = guild.member_get_rank(activator.name)

		# Show the player's rank.
		if rank:
			me.SayTo(activator, "Your rank is: {}".format(rank), 1)

	# Show all ranks.
	elif msg == "ranks":
		me.SayTo(activator, "\nList of ranks:\n")

		for rank in guild.ranks_get_sorted():
			me.SayTo(activator, guild.rank_string(rank), 1)


guild = Guild(GetOptions())
main()
