## @file
## Script for Rogroth temple priests.

from Interface import Interface
import Temple

activator = WhoIsActivator()
me = WhoAmI()
msg = WhatIsMessage().strip().lower()
inf = Interface(activator, me)

def main():
	temple = Temple.TempleRogroth(activator, me, inf)
	temple.handle_chat(msg)

main()
inf.finish()
