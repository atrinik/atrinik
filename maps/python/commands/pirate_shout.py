## @file
## The /pirate_shout command.

from Pirate import english2pirate

if not msg:
	activator.Write("No message given.", COLOR_RED)
else:
	activator.Controller().ExecuteCommand("/shout " + english2pirate(msg))
