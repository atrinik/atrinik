## @file
## Generic script for getting to the map with guild Oracle.
##
## Only allows entrance to guild administrators, DMs are an exception.
##
## Event options can be configured as follows:
##
## - <b>guildname</b>: Will only allow entrance for guild administrators
##   from the guild 'guildname'
## - <b>guildname|10|15</b>: Same as the above, but if the activator is
##   not a guild administrator, he will get teleported to X 10, Y 15 on
##   the current map.

from Atrinik import *
from Guild import Guild

## Activator object.
activator = WhoIsActivator()
## Object who has the event object in their inventory.
me = WhoAmI()

## Get the event options.
event_options = GetOptions().split("|")
## Count the length of the event options.
event_options_length = len(event_options)

# Sanity check
if event_options_length < 1:
	SetReturnValue(1)
	raise error("Invalid event options.")

## Get the guildname.
guildname = event_options[0]

## The guild we're managing.
guild = Guild(guildname)

def main():
	if not guild.is_administrator(activator.name) and not activator.f_wiz:
		activator.Write("Entry forbidden.", COLOR_RED)

		if event_options_length > 2:
			activator.SetPosition(int(event_options[1]), int(event_options[2]))

		SetReturnValue(1)

try:
	main()
finally:
	guild.guilddb.close()
