## @file
## Generic script for smiths in shops.

from Interface import Interface
from Smith import Smith

inf = Interface(activator, me)

def main():
    smith = Smith(activator, me, inf)
    smith.handle_chat(msg)

main()
inf.finish()
