## @file
## Script for the NPC that welcomes new players. Allows players
## to skip the tutorial, if they want to.

from Atrinik import *

activator = WhoIsActivator()
me = WhoAmI()

msg = WhatIsMessage().strip().lower()

if msg == "hello" or msg == "hi" or msg == "hey":
	me.SayTo(activator, "\nWelcome to Atrinik, %s!" % activator.name)

elif msg == "skip tutorial":
	me.SayTo(activator, "\nAre you sure you want to ^skip the tutorial^?")

elif msg == "skip the tutorial":
	me.SayTo(activator, "\nOkay, good luck %s!" % activator.name)
	activator.TeleportTo("/shattered_islands/tutorial_island/tutorial_0100", 22, 1)