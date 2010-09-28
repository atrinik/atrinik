## @file
## Script for Tylowyn temple priests.

from Atrinik import *
import Temple

activator = WhoIsActivator()
me = WhoAmI()
msg = WhatIsMessage().strip().lower()

## Tylowyn.
class TempleTylowyn(Temple.BaseTemple):
	name = "Tylowyn"
	desc = "I am a servant of the first Queen of Elven Kind and Elven Goddess of Luck, Tylowyn.\nIf you would like to join our Temple, please touch the altar and Tylowyn will smile upon you."
	enemy_name = "Dalosha"
	enemy_desc = "That rebellious heretic!  In the days of the First Elven Kings, the first daughter of our gracious Tylowyn sought to overthrow the Elven Kingdoms with her lies and treachery.  After she was routed from the Elven lands, she took her band of rebel dark elves and hid in the caves, but unfortunately managed to survive there.  Avoid those drow if you know what is best for you."

def main():
	Temple.handle_temple(TempleTylowyn, me, activator, msg)

main()
