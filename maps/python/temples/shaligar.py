## @file
## Script for Shaligar temple priests.

from Atrinik import *
import Temple

activator = WhoIsActivator()
me = WhoAmI()
msg = WhatIsMessage().strip().lower()

def main():
	Temple.handle_temple(Temple.TempleShaligar, me, activator, msg)

main()
