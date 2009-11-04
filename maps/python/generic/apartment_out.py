## @file
## Script used by apartment teleporters purchased in Brynknot.

from Atrinik import *
import string, os
from inspect import currentframe

## Activator object.
activator = WhoIsActivator()
## Object who has the event object in their inventory.
me = WhoAmI()

execfile(os.path.dirname(currentframe().f_code.co_filename) + "/apartments.py")

## Name of the apartment we're dealing with.
apartment_id = GetOptions()

if not apartment_id or not apartments_info[apartment_id]:
	activator.TeleportTo("/emergency", 0, 0, 0)
else:
	## The apartment's info
	pinfo = activator.GetPlayerInfo(apartments_info[apartment_id]["tag"])

	if pinfo == None:
		activator.TeleportTo("/emergency", 0, 0, 0)
	else:
		activator.TeleportTo(pinfo.race, pinfo.last_sp, pinfo.last_grace, 0)

SetReturnValue(1)
