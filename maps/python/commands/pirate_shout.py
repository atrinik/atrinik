## @file
## The /pirate_shout command.

from Pirate import english2pirate

try:
	message = CleanupChatString(msg)
except NameError:
	message = None

if not message:
	activator.Write("No message given.", COLOR_RED)
else:
	new_message = english2pirate(message)
	LOG(llevChat, "Pirate shout: {} >{}< >{}<\n".format(activator.name, message, new_message))
	activator.Write("{} shouts: {}".format(activator.name, new_message), COLOR_ORANGE, NDI_ALL | NDI_PLAYER | NDI_SHOUT)
