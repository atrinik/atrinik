## @file
## Guild Oracle provides guild administration features for the guild
## administrators.

from Atrinik import *
from Interface import Interface
from Guild import Guild
import re

inf = Interface(activator, me)
player = activator.Controller()
guild = Guild(GetOptions())

def main():
    if msg == "hello":
        inf.add_msg("Hello {}. Do you want to manage members, the guild itself or the ranks?".format(activator.name))
        inf.add_link("I'd like to manage the members.", dest = "manage_members")
        inf.add_link("I'd like to manage the guild.", dest = "manage_guild")
        inf.add_link("I'd like to manage the ranks.", dest = "manage_ranks")

        if "[OP]" in player.cmd_permissions:
            inf.add_msg("Since you're the Operator, you can also add members directly or change the guild's founder.")
            inf.add_link("I'd like to add a member.", dest = "op_addmember1")
            inf.add_link("I'd like to change the guild's founder.", dest = "op_founder1")

    elif msg == "manage_members":
        inf.add_msg("I can show you the guild's list of applications, or remove a member from the guild.\nOr would you rather give admin status to a member, or perhaps remove admin status from a fellow administrator?")
        inf.add_link("Show me the list of applications.", dest = "member_applications")
        inf.add_link("I'd like to remove a member from the guild.", dest = "member_remove1")
        inf.add_link("I'd like to promote a member to administrator.", dest = "member_admin_give1")
        inf.add_link("I'd like to demote an administrator.", dest = "member_admin_remove1")

    elif msg == "member_applications":
        inf.add_msg("List of membership applications:")

        for member in guild.get_members():
            if not guild.member_approved(member):
                inf.add_msg("{0} ([a=:member_approve {0}]approve[/a], [a=:member_remove2 {0}]decline[/a])")

    elif msg == "member_remove1":
        inf.add_msg("Enter the player's name that you want to remove from the guild.")
        inf.set_text_input(prepend = "member_remove2 ")

    elif msg.startswith("member_remove2 "):
        name = msg[15:].capitalize()

        if not guild.member_exists(name):
            inf.add_msg("No such member {}.".format(name))
        elif not "[OP]" in player.cmd_permissions and name == activator.name:
            inf.add_msg("You cannot remove yourself.")
        elif not "[OP]" in player.cmd_permissions and guild.is_founder(name):
            inf.add_msg("You cannot remove the guild founder.")
        elif guild.member_remove(name):
            inf.add_msg("Successfully removed {} from the guild.".format(name))
        else:
            inf.add_msg("Could not remove {} from the guild.".format(name))

    elif msg.startswith("member_approve "):
        name = msg[15:].capitalize()

        if not guild.member_exists(name):
            inf.add_msg("No such member {}.".format(name))
        elif guild.member_approved(name):
            inf.add_msg("This member has already been approved.")
        elif guild.member_approve(name):
            inf.add_msg("Successfully approved {} for full guild membership.".format(name))
        else:
            inf.add_msg("Could not approve {}.".format(name))

    elif msg == "member_admin_give1":
        inf.add_msg("Enter the player's name that you want to promote to guild administrator.")
        inf.set_text_input(prepend = "member_admin_give2 ")

    elif msg.startswith("member_admin_give2 "):
        name = msg[19:].capitalize()

        if not guild.member_exists(name):
            inf.add_msg("No such member {}.".format(name))
        elif guild.member_is_admin(name):
            inf.add_msg("This member is already an administrator.")
        elif guild.member_admin_make(name):
            inf.add_msg("Successfully made {} a guild administrator.".format(name))
        else:
            inf.add_msg("{} cannot be made an administrator.".format(name))

    elif msg == "member_admin_remove1":
        inf.add_msg("Enter the player's name that you want to demote from guild administrator to normal member.")
        inf.set_text_input(prepend = "member_admin_remove2 ")

    elif msg.startswith("member_admin_remove2 "):
        name = msg[21:].capitalize()

        if not guild.member_exists(name):
            inf.add_msg("No such member {}.".format(name))
        elif not guild.member_is_admin(name):
            inf.add_msg("This member is not an administrator.")
        elif not "[OP]" in player.cmd_permissions and name == activator.name:
            inf.add_msg("You cannot take away administrator rights from yourself.")
        elif not "[OP]" in player.cmd_permissions and guild.is_founder(name):
            inf.add_msg("You cannot take administrator rights from the guild founder.")
        elif guild.member_admin_remove(name):
            inf.add_msg("Successfully removed administrator rights from {}.".format(name))
        else:
            inf.add_msg("{} cannot have administrator rights taken away.".format(name))

    elif msg == "manage_guild":
        closed = guild.guild_check(guild.guild_closed)
        inf.add_msg("The guild is currently {}.".format("closed" if closed else "open"))

        if closed:
            inf.add_link("Open the guild.", dest = "guild_open")
        else:
            inf.add_link("Close the guild.", dest = "guild_close")

    elif msg == "guild_open":
        guild.guild_unset(guild.guild_closed)
        inf.add_msg("Opened the guild. New membership applications will be accepted by the Guild Manager.")

    elif msg == "guild_close":
        guild.guild_unset(guild.guild_closed)
        inf.add_msg("Opened the guild. New membership applications will be accepted by the Guild Manager.")

    elif msg == "manage_ranks":
        inf.add_msg("List of ranks:")

        for rank in guild.ranks_get_sorted():
            inf.add_msg(guild.rank_string(rank))
            inf.add_msg("\n([a=:rank_add_member1 \"{0}\"]add member[/a], [a=:rank_change1 \"{0}\"]change setting[/a])".format(rank), newline = False)

        no_rank_members = list(filter(lambda name: not guild.member_get_rank(name), guild.get_members()))

        if no_rank_members:
            inf.add_msg("[green]No rank assigned[/green]\n{}".format(", ".join(no_rank_members)))

        inf.add_link("I'd like to add a rank.", dest = "rank_add1")
        inf.add_link("I'd like to delete a rank.", dest = "rank_delete1")

    elif msg.startswith("rank_add_member1 "):
        inf.add_msg("Enter the player's name that you want to add to the specified rank.")
        inf.set_text_input(prepend = "rank_add_member2 " + msg[17:] + " ")

    elif msg.startswith("rank_add_member2 "):
        match = re.match(r"\"([^\"]+)\" (.+)", msg[17:])

        if not match:
            return

        (rank, name) = match.groups()
        name = name.capitalize()

        if not guild.rank_exists(rank):
            inf.add_msg("No such rank '{}'.".format(rank))
        elif not guild.member_exists(name):
            inf.add_msg("No such member {}.".format(name))
        elif guild.member_set_rank(name, rank):
            inf.add_msg("Added {} to rank '{}'.".format(name, rank))
        else:
            inf.add_msg("Could not add {} to rank '{}'".format(name, rank))

    elif msg.startswith("rank_change1 "):
        rank = msg[13:]
        inf.add_msg("Which setting of that rank would you like to change?")
        inf.add_link("Change limit value.", dest = "rank_change2 " + rank + " limit_value")
        inf.add_link("Change limit reset time.", dest = "rank_change2 " + rank + " limit_time")

    elif msg.startswith("rank_change2 "):
        match = re.match(r"\"([^\"]+)\" (\w+)", msg[13:])

        if not match:
            return

        (rank, what) = match.groups()

        if what == "limit_value":
            inf.add_msg("Enter the value limit for that rank, such as [green]6 gold[/green] or [green]1 m 500 g[/green] or [green]unlimited[/green].")
        elif what == "limit_time":
            inf.add_msg("Enter the amount of time when to reset the value limit of that rank, in hours.")

        inf.set_text_input(prepend = "rank_change3 \"" + rank + "\" " + what + " ")

    elif msg.startswith("rank_change3 "):
        match = re.match(r"\"([^\"]+)\" (\w+) (.+)", msg[13:])

        if not match:
            return

        (rank, what, value) = match.groups()

        if not guild.rank_exists(rank):
            return

        if what == "limit_value":
            from Auction import string_to_cost

            amount = string_to_cost(value)

            if amount > guild.rank_value_max:
                inf.add_msg("Invalid value to set, must be 0-{}.".format(guild.rank_value_max))
            elif guild.rank_set(rank, "value_limit", amount):
                inf.add_msg("Successfully changed value limit to {}.".format(CostString(amount)))
            else:
                inf.add_msg("Could not change value limit.")
        elif what == "limit_time":
            if not value.isdigit() or int(value) < guild.rank_reset_min or int(value) > guild.rank_reset_max:
                inf.add_msg("Invalid value to set, must be {}-{}.".format(guild.rank_reset_min, guild.rank_reset_max))
            elif guild.rank_set(rank, "value_reset", int(value)):
                inf.add_msg("Successfully changed limit reset time to {} hour(s).".format(value))
            else:
                inf.add_msg("Could not change limit reset time.".format(value))

    elif msg == "rank_add1":
        inf.add_msg("Enter the rank name that you want to add.")
        inf.set_text_input(prepend = "rank_add2 ")

    elif msg.startswith("rank_add2 "):
        rank = guild.rank_sanitize(msg[10:]).title()

        if not rank:
            inf.add_msg("Invalid rank name: it cannot contain special symbols/characters and the maximum length is {} characters.".format(guild.get(guild.rank_max_chars)))
        elif guild.rank_exists(rank):
            inf.add_msg("The rank '{}' already exists.".format(rank))
        elif guild.rank_add(rank):
            inf.add_msg("Successfully created rank '{}'.".format(rank))
        else:
            inf.add_msg("Could not add rank '{}'.".format(rank))

    elif msg == "rank_delete1":
        inf.add_msg("Enter the rank name that you want to delete.")
        inf.set_text_input(prepend = "rank_delete2 ")

    elif msg.startswith("rank_delete2 "):
        rank = msg[13:].title()

        if not guild.rank_exists(rank):
            inf.add_msg("No such rank '{}'.".format(rank))
        elif guild.rank_remove(rank):
            inf.add_msg("Removed rank '{}'.".format(rank))
        else:
            inf.add_msg("Rank '{}' could not be removed.".format(rank))

    elif msg.startswith("op_") and "[OP]" in player.cmd_permissions:
        if msg == "op_addmember1":
            inf.add_msg("Enter the player's name that you want to add to the guild.")
            inf.set_text_input(prepend = "op_addmember2 ")

        elif msg.startswith("op_addmember2 "):
            name = msg[14:].capitalize()

            if not PlayerExists(name):
                inf.add_msg("No such player.")
            elif guild.member_exists(name):
                inf.add_msg("{} is already a member of this guild.".format(name))
            else:
                guild.member_add(name)
                inf.add_msg("Successfully added {} to the guild.".format(name))

        elif msg == "op_founder1":
            inf.add_msg("Enter the player's name that you want to make the founder of this guild.")
            inf.set_text_input(prepend = "op_founder2 ")

        elif msg.startswith("op_founder2 "):
            name = msg[12:].capitalize()

            if not PlayerExists(name):
                inf.add_msg("No such player.")
            elif guild.set_founder(name):
                inf.add_msg("Successfully made {} the guild founder.".format(name))
            else:
                inf.add_msg("Could not make {} the guild founder.".format(name))

main()
inf.finish()
