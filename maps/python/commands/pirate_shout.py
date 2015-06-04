## @file
## The /pirate_shout command.

from Atrinik import *
from Pirate import english2pirate

if not msg:
    pl.DrawInfo("No message given.", COLOR_RED)
else:
    pl.ExecuteCommand("/shout " + english2pirate(msg))
