## @file
## Script for Drolaxi temple priests.

from Interface import Interface
import Temple

inf = Interface(activator, me)

def main():
	Temple.handle_temple(Temple.TempleDrolaxi, me, activator, msg, inf)
	temple = Temple.TempleDrolaxi(activator, me, inf)
	temple.handle_chat(msg)

main()
inf.finish()
