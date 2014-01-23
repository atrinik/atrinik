## @file
## Generic bank script used for bank NPCs.

from Interface import Interface
from Bank import Bank

inf = Interface(activator, me)

def main():
    bank = Bank(activator, me, inf)
    bank.handle_chat(msg)

main()
inf.finish()
