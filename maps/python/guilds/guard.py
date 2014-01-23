## @file
## Simple guild greeter, handles showing player's guild rank and telling
## about all the ranks available.

from Interface import Interface
from Guild import Guild

inf = Interface(activator, me)
guild = Guild(GetOptions())

def main():
    if msg == "hello":
        rank = guild.member_get_rank(activator.name)

        inf.add_msg("Welcome to the guild, {}.".format(activator.name))

        if rank:
            inf.add_msg("Your rank is: {}".format(rank))

        inf.add_msg("Would you like to see the ranks list?")
        inf.add_link("Yes, please.", dest = "ranks")

    elif msg == "ranks":
        inf.add_msg("List of ranks:")

        for rank in guild.ranks_get_sorted():
            inf.add_msg(guild.rank_string(rank))


main()
inf.finish()
