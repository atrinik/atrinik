## @file
## Script for Terria temple priests.

from Interface import Interface
import Temple

inf = Interface(activator, me)

def main():
    temple = Temple.TempleTerria(activator, me, inf)
    temple.handle_chat(msg)

main()
inf.finish()
