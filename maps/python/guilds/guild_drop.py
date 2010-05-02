## @file
## Used when dropping special objects like guild chests and guild signs.
##
## This basically only allows dropping them in guild oracle or guild
## storage.

from Atrinik import *

activator = WhoIsActivator()
me = WhoAmI()

floor = activator.map.GetLastObject(activator.x, activator.y)

if me.f_unpaid and (not floor or floor.type != TYPE_SHOP_FLOOR):
	activator.Write("You must pay for it first!", COLOR_RED)
	SetReturnValue(1)
elif not activator.map.path[:8] == "/guilds/" or (activator.map.path[-7:] != "/oracle" and activator.map.path[-8:] != "/storage"):
	activator.Write("You cannot drop this object here.", COLOR_RED)
	SetReturnValue(1)
