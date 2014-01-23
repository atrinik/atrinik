## @file
## Generic script to handle merchants.

from Interface import Interface
from Market import Merchant

inf = Interface(activator, me)

def main():
    merchant = Merchant(activator, me, WhatIsEvent(), inf)
    merchant.handle_msg(msg)

main()
inf.finish()
