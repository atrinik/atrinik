## @file
## Script for Rogroth temple priests.

from Atrinik import *
import Temple

activator = WhoIsActivator()
me = WhoAmI()
msg = WhatIsMessage().strip().lower()

## Rogroth.
class TempleRogroth(Temple.BaseTemple):
	name = "Rogroth"
	desc = "I am a servant of the King of the Stormy Skies and the God of Lightning, Rogroth.\nIf you would like to join our Temple, please touch the altar and Drolaxi will smile upon you."

def main():
	Temple.handle_temple(TempleRogroth, me, activator, msg)

main()
