## @file
## Script for Moroch temple priests.

from Atrinik import *
import Temple

activator = WhoIsActivator()
me = WhoAmI()
msg = WhatIsMessage().strip().lower()

## Moroch.
class TempleMoroch(Temple.BaseTemple):
	name = "Moroch"
	desc = "I am a servant of the Lord of the Grave and King of Undeath, Moroch.\nIf you would like to join our Temple, please touch the altar and Moroch will smile upon you."
	enemy_name = "Terria"
	enemy_desc = "Do you honestly believe the lies of those naturists?  The powers of undeath will rule the universe and the servants of Nature will fail.  The Dark Lord shall not fail to dominate the land and all be consumed in glorious Death."

def main():
	Temple.handle_temple(TempleMoroch, me, activator, msg)

main()
