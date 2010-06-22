## @file
## Generic apartment teleporter script.
##
## Used to teleport apartment owners to their apartment.

from Atrinik import *

## Activator object.
activator = WhoIsActivator()
## Object who has the event object in their inventory.
me = WhoAmI()

exec(open(CreatePathname("/python/generic/apartments.py")).read())

## Name of the apartment we're dealing with.
apartment_id = GetOptions()

if not apartment_id or not apartments_info[apartment_id]:
	activator.SetPosition(me.hp, me.sp)
else:
	## The apartments we're dealing with.
	apartments = apartments_info[apartment_id]["apartments"]

	## The apartment's info
	pinfo = activator.GetPlayerInfo(apartments_info[apartment_id]["tag"])

	# No apartment, teleport them back
	if pinfo == None:
		activator.Write("You don't own an apartment here!", 0)
		activator.SetPosition(me.hp, me.sp)
	else:
		if apartments[pinfo.slaying]:
			# The apartment info
			apartment_info = apartments[pinfo.slaying]

			activator.TeleportTo(apartment_info["path"], apartment_info["x"], apartment_info["y"], 1)

			pinfo.race = me.map.path
			pinfo.last_sp = me.hp
			pinfo.last_grace = me.sp
		else:
			activator.SetPosition(activator.me.hp, me.sp)

SetReturnValue(1)
