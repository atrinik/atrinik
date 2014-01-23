## @file
## Script for Rashindel temple priests.

from Interface import Interface
import Temple

inf = Interface(activator, me)

def main():
    temple = Temple.TempleRashindel(activator, me, inf)
    temple.handle_chat(msg)

main()
inf.finish()
