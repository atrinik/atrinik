## @file
## The /pirate_say command.

from Pirate import english2pirate

try:
	message = CleanupChatString(msg)
except NameError:
	message = None

if not message:
	activator.Write("No message given.", COLOR_RED)
else:
	new_message = english2pirate(message)
	Logger("CHAT", "Pirate say: {} >{}< >{}<".format(activator.name, message, new_message))
	activator.Communicate(new_message)
