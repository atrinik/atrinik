## @file
## Implements Miyoko, the potion seller's script.

from Interface import Interface
from Market import Merchant

inf = Interface(activator, me)

def main():
    lower_msg = msg.lower()
    if lower_msg == "marisa" or lower_msg == "kirisame" or lower_msg == "marisa kirisame":
        inf.add_msg("Yeah, she's pretty awesome.")
    elif lower_msg == "takano" or lower_msg == "miyo takano" or lower_msg == "tanishi" or lower_msg == "miyoko tanishi" or lower_msg == "higurashi no naku koro ni":
        inf.add_msg("Hihihi,  Good job guessing where my name is from.  Don't worry, I won't turn out like her...")
    else:
        merchant = Merchant(activator, me, WhatIsEvent(), inf)
        merchant.handle_msg(msg)

main()
inf.finish()
