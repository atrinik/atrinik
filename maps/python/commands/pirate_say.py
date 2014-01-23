## @file
## The /pirate_say command.

from Pirate import english2pirate

if not msg:
    pl.DrawInfo("No message given.", COLOR_RED)
else:
    pl.ExecuteCommand("/say " + english2pirate(msg))
