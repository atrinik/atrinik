## @file
## Python script to set return value of magic mouths (and anything that
## says message on activation by walking).
##
## Usage:
##
## - Make a magic mouth
## - Insert python event object inside it
## - Set the event's script path to this script
## - Make the event execute on trigger
## - For event options choose comma separate list of directions you want
##   the message to be shown.

from Atrinik import *

## Activator for facing direction
activator = WhoIsActivator()
## Get the directions from the event's options
directions = GetOptions().split(",")

# Looks for the activator's facing inside directions and convert the
# facing to string. If not found, set return value to 1.
if not str(activator.facing) in directions:
	SetReturnValue(1)
