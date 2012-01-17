## @file
## The /pirate_say command.

from Pirate import english2pirate

if not msg:
	activator.Write("No message given.", COLOR_RED)
else:
	activator.Controller().ExecuteCommand("/say " + english2pirate(msg))
