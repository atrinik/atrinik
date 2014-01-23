## @file
## Generic script to handle bartenders, using the Tavern API.

from Interface import Interface
from Tavern import Bartender

inf = Interface(activator, me)

def main():
    bartender = Bartender(activator, me, WhatIsEvent(), inf)
    bartender.handle_msg(msg)

main()
inf.finish()
