## @file
## Script for Jotarl temple priests.

from Interface import Interface
import Temple

inf = Interface(activator, me)

def main():
    temple = Temple.TempleJotarl(activator, me, inf)
    temple.handle_chat(msg)

main()
inf.finish()
