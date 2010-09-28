## @file
## Script for Shaligar temple priests.

from Atrinik import *
import Temple

activator = WhoIsActivator()
me = WhoAmI()
msg = WhatIsMessage().strip().lower()

## TempleShaligar.
class TempleShaligar(Temple.BaseTemple):
	name = "Shaligar"
	desc = "I am a servant of King of the Lava and the God of Flame, Shaligar.\nIf you would like to join our Temple, please touch the altar and Shaligar will smile upon you."
	enemy_name = "Drolaxi"
	enemy_desc = "Ah, the weak and cowardly sister of the Flame Lord.  One day, she shall no longer be able to keep our flames from consuming all things and our flames shall make all subjects to our will."

def main():
	Temple.handle_temple(TempleShaligar, me, activator, msg)

main()
