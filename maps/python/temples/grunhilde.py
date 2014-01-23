## @file
## Script for Grunhilde temple priests.

from Interface import Interface
import Temple

inf = Interface(activator, me)

def main():
    temple = Temple.TempleGrunhilde(activator, me, inf)
    temple.handle_chat(msg)

main()
inf.finish()
