## @file
## This script manages the /guild command registered when Python plugin
## finished loading.
##
## The command provides a chat for guild members.

from Atrinik import *
from Guild import Guild
from Common import player_sanitize_input

guild = Guild(None)

def main():
    # Check which guild the player is member of.
    g = guild.pl_get_guild(activator.name)

    if not g or not g[2]:
        pl.DrawInfo("You are not member of any guild.", COLOR_RED)
        return

    message = player_sanitize_input(msg)

    if not message:
        pl.DrawInfo("You must provide a message to send to other guild members.", COLOR_RED)
        return

    # Simply switch the guild name we are managing.
    guild.set(g[0])
    guildname = guild.get_name()
    Logger("CHAT", "Guild: {0} [{1}] >{2}<".format(activator.name, guildname, message))

    for member in guild.get_members():
        if not guild.member_approved(member):
            continue

        # Find the member, and if found, show him the guild message.
        player = FindPlayer(member)

        if player:
            player.Controller().DrawInfo(message, COLOR_BLUE, CHAT_TYPE_GUILD, name = activator.name)

main()
