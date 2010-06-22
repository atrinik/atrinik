## @file
## Sanity checking file for guild entrance exit. This script will check
## if activator is a member of specified guild, and will not allow entry
## to those who are not members.
##
## Also allows entrance to DMs.

from Atrinik import *
from Guild import Guild

## Activator object.
activator = WhoIsActivator()

## Get the guild name from the event options.
guildname = GetOptions()

# Sanity check
if not guildname:
	raise error("Guild entrance is missing event options.")

## The guild we're managing.
guild = Guild(guildname)

def main():
	if not activator.f_wiz and not guild.is_member_of(activator.name, guildname):
		activator.Write("You are not member of this guild. You may not enter.", COLOR_RED)
		SetReturnValue(1)

try:
	main()
finally:
	guild.guilddb.close()
