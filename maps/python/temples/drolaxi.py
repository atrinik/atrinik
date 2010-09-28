## @file
## Script for Drolaxi temple priests.

from Atrinik import *
import Temple

activator = WhoIsActivator()
me = WhoAmI()
msg = WhatIsMessage().strip().lower()

## Drolaxi.
class TempleDrolaxi(Temple.BaseTemple):
	name = "Drolaxi"
	desc = "I am a servant of Queen of the Chaotic Seas and the Goddess of Water, Drolaxi.\nIf you would like to join our Temple, please touch the altar and Drolaxi will smile upon you."
	enemy_name = "Shaligar"
	enemy_desc = "Flames and terror does he seek to spread.  Do not be deceived, although the flame be kin to the Lady, he is complerely mad.  Avoid the scorching flames or they will consume you.  We shall rule the world and all shall be seas!"

def main():
	Temple.handle_temple(TempleDrolaxi, me, activator, msg)

main()
