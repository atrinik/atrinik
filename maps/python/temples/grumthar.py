## @file
## Script for Grumthar temple priests.

from Atrinik import *
import Temple

activator = WhoIsActivator()
me = WhoAmI()
msg = WhatIsMessage().strip().lower()

def main():
	Temple.handle_temple(Temple.TempleGrumthar, me, activator, msg)

main()
