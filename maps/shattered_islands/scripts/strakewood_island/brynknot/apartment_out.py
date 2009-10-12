## @file
## Script used by apartment teleporters purchased in Brynknot.
#
## @todo In the future this would allow single apartment on an island,
## but to be able to enter it from all towns, and leave the apartment to
## be teleported back to the town they came to the apartment from.

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

if pinfo == None:
	activator.TeleportTo("/emergency", 0, 0, 0)
else:
	activator.TeleportTo(pinfo.race, pinfo.last_sp, pinfo.last_grace, 0)

SetReturnValue(1)
