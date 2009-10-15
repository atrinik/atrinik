## @file
## This script is used for exits in guild maps, in order to check whether
## the activator is still a member of the guild. If not, they are
## teleported out of the guild.

from Atrinik import *
from imp import load_source
import string

## Activator object.
activator = WhoIsActivator()
## Object who has the event object in their inventory.
me = WhoAmI()

## Get the guild name from the event options.
event_options = GetOptions().split("|")
## Get the guild name
guildname = event_options[0]
## Get the X position where to teleport the player if he is member of the
## guild.
x = event_options[1]
## Get the Y position where to teleport the player if he is member of the
## guild.
y = event_options[2]

## The Guild class.
Guild = load_source("Guild", CreatePathname("/python/Guild.py"))
## The guild we're managing.
guild = Guild.Guild(guildname)

if not guild.is_member_of(activator.name, guildname):
	activator.Write("You have been removed from the guild while you were offline. Goodbye!", COLOR_RED)
	guild.remove_player_from_guild_maps(activator, me)
elif activator.map.path[-7:] == "/oracle" and not guild.is_administrator(activator.name):
	activator.Write("You have had administrator rights taken away while you were offline.", COLOR_RED)
	guild.remove_player_from_guild_maps(activator, me)
else:
	activator.SetPosition(int(x), int(y))
