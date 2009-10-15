## @file
## Sanity checking file for guild entrance exit. This script will check
## if activator is a member of specified guild, and will not allow entry
## to those who are not members.
##
## Also allows entrance to DMs.

from Atrinik import *
from imp import load_source

## Activator object.
activator = WhoIsActivator()

## Get the guild name from the event options.
guildname = GetOptions()

# Sanity check
if guildname:
	## The Guild class.
	Guild = load_source("Guild", CreatePathname("/python/Guild.py"))
	## The guild we're managing.
	guild = Guild.Guild(guildname)

	if not activator.f_wiz and not guild.is_member_of(activator.name, guildname):
		activator.Write("You are not member of this guild. You may not enter.", COLOR_RED)
		SetReturnValue(1)
