# Python script to set return value of magic mouths (and
# anything that says message on activation by walking).
# By Alex Tokar (Cleo), 2009.
#
# Usage:
#  - Make a magic mouth
#  - Insert python event object inside it
#  - Set the event's script path to this script
#  - Make the event execute on trigger
#  - For event options choose comma separate list of
#    directions you want the message to be shown.

import Atrinik
import string

# Activator for facing direction
activator = Atrinik.WhoIsActivator()
# Get the directions from the event's options
directions = Atrinik.GetOptions().split(",")

# Looks for the activator's facing inside directions
# and convert the facing to string. If not found,
# Set return value to 1.
if not str(activator.facing) in directions:
	Atrinik.SetReturnValue(1)

