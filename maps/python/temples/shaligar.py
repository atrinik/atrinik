## @file
## Script for Shaligar temple priests.

from Interface import Interface
import Temple

inf = Interface(activator, me)

def main():
    temple = Temple.TempleShaligar(activator, me, inf)
    temple.handle_chat(msg)

main()
inf.finish()
