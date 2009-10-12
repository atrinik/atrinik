## @file
## Apartment teleporter in Brynknot.
##
## Used to teleport apartment owners to their apartment.

from Atrinik import *
import string, os
from inspect import currentframe

## Activator object.
activator = WhoIsActivator()
## Object who has the event object in their inventory.
me = WhoAmI()

execfile(os.path.dirname(currentframe().f_code.co_filename) + "/apartments.py")

## The apartment's info
pinfo = activator.GetPlayerInfo(apartment_tag)

# No apartment, teleport them back
if pinfo == None:
	me.SayTo(activator, "You don't own an apartment here!");
	activator.Write("A strong force teleports you away.", 0)
	activator.SetPosition(8, 4)
else:
	if apartments[pinfo.slaying]:
		# The apartment info
		apartment_info = apartments[pinfo.slaying]

		activator.TeleportTo(apartment_info["path"], apartment_info["x"], apartment_info["y"], 1)

		pinfo.race = me.map.path
		pinfo.last_sp = me.hitpoints
		pinfo.last_grace = me.spellpoints
	else:
		me.SayTo(activator, "Wrong apartment ID?!");
		activator.Write("A strong force teleports you away.", 0)
		activator.SetPosition(8, 4)

SetReturnValue(1)
