## @file
## Dialog for the witch in swamp near Asteria.

from Atrinik import *

me = WhoAmI()
activator = WhoIsActivator()
msg = WhatIsMessage().strip().lower()

def main():
	# Don't say anything if we have an enemy; she's focusing on the enemy.
	if me.enemy:
		return

	if msg == "hi" or msg == "hey" or msg == "hello":
		me.SayTo(activator, "\nWelcome, welcome... Heh-heh-heh...\n\n^Why do you have 2 brooms?^")

	elif msg == "why do you have 2 brooms?":
		me.SayTo(activator, "\nThat is none of your concern! Now go away before I get really mad!\n\n^Did you steal one?^")

	elif msg == "did you steal one?":
		me.SayTo(activator, "\nThat's it! Now you die!")
		# The witch gets angry...
		me.enemy = activator
		# Find witch's cat spawn point.
		obj = me.map.GetLastObject(14, 12)

		# Check if it really is a spawn point and whether there is a spawned
		# monster, if so, set the spawned monster's enemy to activator too.
		if obj.type == TYPE_SPAWN_POINT and obj.enemy:
			obj.enemy.enemy = activator

main()
