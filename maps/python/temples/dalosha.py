## @file
## Script for Dalosha temple priests.

from Atrinik import *
import Temple

activator = WhoIsActivator()
me = WhoAmI()
msg = WhatIsMessage().strip().lower()

## Dalosha.
class TempleDalosha(Temple.BaseTemple):
	name = "Dalosha"
	desc = "I am a servant of the first Queen of the Drow and Spider Goddess, Dalosha.\nIf you would like to join our Temple, please touch the altar and Dalosha will smile upon you."
	enemy_name = "Tylowyn"
	enemy_desc = "The high elves and their oppressive queen!  Do not be swayed by her traps, she started the war with her attempt to enforce proper elven conduct in war.  Tylowyn was too cowardly and weak to realize that it was our destiny to rule the world, so now she and her elves shall also perish!"

def main():
	Temple.handle_temple(TempleDalosha, me, activator, msg)

main()
