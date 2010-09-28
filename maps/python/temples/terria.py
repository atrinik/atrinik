## @file
## Script for Terria temple priests.

from Atrinik import *
import Temple

activator = WhoIsActivator()
me = WhoAmI()
msg = WhatIsMessage().strip().lower()

## Terria.
class TempleTerria(Temple.BaseTemple):
	name = "Terria"
	desc = "I am a servant of Mother Earth and the Goddess of Life, Terria.\nIf you would like to join our Temple, please touch the altar and Terria will smile upon you."
	enemy_name = "Moroch"
	enemy_desc = "Speak not of the Dark Lord here!  The King of Death with his awful necromantic minions that rise from the sleep of death are not to be trifled with, for they are dangerous.  Our Lady has long sought to remove the plague of death from the lands after that foul Lich ascended."

def main():
	Temple.handle_temple(TempleTerria, me, activator, msg)

main()
