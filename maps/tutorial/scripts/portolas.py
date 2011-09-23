## @file
## Script for the NPC that welcomes new players. Allows players
## to skip the tutorial, if they want to.

from Atrinik import *

activator = WhoIsActivator()
me = WhoAmI()
msg = WhatIsMessage().strip().lower()

def main():
	if msg == "hello" or msg == "hi" or msg == "hey":
		me.SayTo(activator, "\nWelcome to Atrinik, {}!".format(activator.name))

	elif msg == "skip tutorial":
		me.SayTo(activator, "\nAre you sure you want to <a>skip the tutorial</a>?")

	elif msg == "skip the tutorial":
		import os

		me.SayTo(activator, "\nOkay, good luck {}!".format(activator.name))
		activator.TeleportTo("{}/{}".format(os.path.dirname(me.map.path), "tutorial_0302"), 6, 21)

main()
