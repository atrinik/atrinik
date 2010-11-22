## @file
## Script for Tylowyn temple priests.

from Atrinik import *
import Temple

activator = WhoIsActivator()
me = WhoAmI()
msg = WhatIsMessage().strip().lower()

def main():
	Temple.handle_temple(Temple.TempleTylowyn, me, activator, msg)

main()
