## @file
## Script for Grumthar temple priests.

from Atrinik import *
import Temple

activator = WhoIsActivator()
me = WhoAmI()
msg = WhatIsMessage().strip().lower()

## Grumthar.
class TempleGrumthar(Temple.BaseTemple):
	name = "Grumthar"
	desc = "I am a servant of the First Dwarven Lord and the God of Smithery, Grumthar.\nIf you would like to join our Temple, please touch the altar and Grumthar will smile upon you."
	enemy_name = "Jotarl"
	enemy_desc = "Do not be speaking of that Giant tyrant amongst us.  Him and his giants have long sought to crush the little folk.  He has those goblin vermin under his wing also."

def main():
	Temple.handle_temple(TempleGrumthar, me, activator, msg)

main()
