## @file
## Script for Grunhilde temple priests.

from Atrinik import *
import Temple

activator = WhoIsActivator()
me = WhoAmI()
msg = WhatIsMessage().strip().lower()

## Grunhilde.
class TempleGrunhilde(Temple.BaseTemple):
	name = "Grunhilde"
	desc = "I am a servant of the Valkyrie Queen and the Goddess of Victory, Grunhilde.\nIf you would like to join our Temple and fight for our cause, please touch the altar and Grunhilde will smile upon you."

def main():
	Temple.handle_temple(TempleGrunhilde, me, activator, msg)

main()
