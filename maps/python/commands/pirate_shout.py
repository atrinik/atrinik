## @file
## The /pirate_shout command.

from Pirate import english2pirate
from Common import player_sanitize_input

message = player_sanitize_input(msg)

if not message:
	activator.Write("No message given.", COLOR_RED)
else:
	new_message = english2pirate(message)
	Logger("CHAT", "Pirate shout: {} >{}< >{}<".format(activator.name, message, new_message))
	activator.Write("{} shouts: {}".format(activator.name, new_message), COLOR_ORANGE, NDI_ALL | NDI_PLAYER | NDI_SHOUT)
