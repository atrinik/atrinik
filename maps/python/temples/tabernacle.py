## @file
## Script for Tabernacle temple priests.

from Interface import Interface
import Temple

activator = WhoIsActivator()
me = WhoAmI()
msg = WhatIsMessage().strip().lower()
inf = Interface(activator, me)

def main():
	temple = Temple.TempleTabernacle(activator, me, inf)
	temple.handle_chat(msg)

main()
inf.finish()
