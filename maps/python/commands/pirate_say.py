## @file
## The /pirate_say command.

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
	LOG(llevInfo, "CLOG PIRATE_SAY: {0} >{1}< >{2}<\n".format(activator.name, message, new_message))
	activator.Communicate(new_message)
