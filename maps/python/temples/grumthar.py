## @file
## Script for Grumthar temple priests.

from Interface import Interface
import Temple

inf = Interface(activator, me)

def main():
    temple = Temple.TempleGrumthar(activator, me, inf)
    temple.handle_chat(msg)

main()
inf.finish()
