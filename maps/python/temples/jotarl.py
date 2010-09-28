## @file
## Script for Jotarl temple priests.

from Atrinik import *
import Temple

activator = WhoIsActivator()
me = WhoAmI()
msg = WhatIsMessage().strip().lower()

## Jotarl.
class TempleJotarl(Temple.BaseTemple):
	name = "Jotarl"
	desc = "I am a servant of the Titan King and the God of the Giants, Jotarl.\nIf you would like to join our Temple, please touch the altar and Jotarl will smile upon you."
	enemy_name = "Grumthar"
	enemy_desc = "Puny dwarves do not scare Jotarl with their technology and mithril weapons, we shall rule the caves!  The Dwarves shall fall and we shall claim their gold for ourselves."

def main():
	Temple.handle_temple(TempleJotarl, me, activator, msg)

main()
