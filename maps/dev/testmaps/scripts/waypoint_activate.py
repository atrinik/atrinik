## @file
## Test to trigger waypoints from scripts

from Atrinik import *

activator = WhoIsActivator()
me = WhoAmI()
msg = WhatIsMessage().strip().lower()

def main():
	if msg == "go":
		tmp = me.FindObject(0, None, "waypoint1")

		if tmp == None:
			me.SayTo(activator, "Oops... I seem to be lost.")
		else:
			# This activates the waypoint
			tmp.f_cursed = True
	else:
		me.SayTo(activator, "Tell me to ^go^ to activate my waypoint.")

main()
