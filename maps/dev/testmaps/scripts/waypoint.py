## @file
## Just a basic test of waypoint triggers

from Atrinik import *

## The monster
activator = WhoIsActivator()
## The waypoint
me = WhoAmI()

# Do something
activator.Say("I just reached the waypoint \"{0}\"".format(me.name))
