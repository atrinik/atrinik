## @file
## Script for Rashindel temple priests.

from Atrinik import *
import Temple

activator = WhoIsActivator()
me = WhoAmI()
msg = WhatIsMessage().strip().lower()

## Rashindel.
class TempleRashindel(Temple.BaseTemple):
	name = "Rashindel"
	desc = "I am a servant of the Demonic King and the Overlord of Hell, Rashindel.\nIf you would like to join our Circle, please touch the altar and Rashindel will smile upon you."
	enemy_name = "Tabernacle"
	enemy_desc = "Accursed fool, do not mention that name in our presence!  In the days before this world, the Tyrant sought to oppress us with the his oppressive ideals of truth and justice.  After our master freed us from the simpleton lots who follow him, he was bound into the darkness which is now our glorious kingdom."

def main():
	Temple.handle_temple(TempleRashindel, me, activator, msg)

main()
