## @file
## Used when dropping special objects like guild chests and guild signs.
##
## This basically only allows dropping them in guild oracle or guild
## storage.

from Atrinik import *

## Activator object.
activator = WhoIsActivator()
## Object who has the event object in their inventory.
me = WhoAmI()

if not activator.map.path[:8] == "/guilds/" or (activator.map.path[-7:] != "/oracle" and activator.map.path[-8:] != "/storage"):
	activator.Write("You cannot drop this object here.", COLOR_RED)
	SetReturnValue(1)
