## @file
## Script to run once player exits the tutorial cave, to set the player's
## save bed.

from Atrinik import *

WhoIsActivator().SetSaveBed(ReadyMap("/_incuna", 0), 103, 59)