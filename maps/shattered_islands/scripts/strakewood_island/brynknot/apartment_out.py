from Atrinik import * 
import string, os
from inspect import currentframe

activator = WhoIsActivator()
me = WhoAmI()

execfile(os.path.dirname(currentframe().f_code.co_filename) + "/apartments.py")

# The apartment's info
pinfo = activator.GetPlayerInfo(apartment_tag)

if pinfo == None:
	activator.TeleportTo("/emergency", 0, 0, 0)
else:
	activator.TeleportTo(pinfo.race, pinfo.last_sp, pinfo.last_grace, 0)

SetReturnValue(1)
