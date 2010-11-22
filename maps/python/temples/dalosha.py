## @file
## Script for Dalosha temple priests.

from Atrinik import *
import Temple

activator = WhoIsActivator()
me = WhoAmI()
msg = WhatIsMessage().strip().lower()

def main():
	Temple.handle_temple(Temple.TempleDalosha, me, activator, msg)

main()
