## @file
## The /pirate_shout command.

from Atrinik import *
from Pirate import english2pirate

activator = WhoIsActivator()

try:
	message = CleanupChatString(WhatIsMessage())
except:
	message = None

if not message:
	activator.Write("No message given.", COLOR_RED)
else:
	new_message = english2pirate(message)
	LOG(llevInfo, "CLOG PIRATE_SHOUT: {0} >{1}< >{2}<\n".format(activator.name, message, new_message))
	activator.Write("{0} shouts: {1}".format(activator.name, new_message), COLOR_ORANGE, NDI_ALL | NDI_PLAYER | NDI_SHOUT)
