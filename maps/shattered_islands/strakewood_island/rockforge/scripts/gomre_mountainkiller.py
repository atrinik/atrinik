## @file
## Script for Gomre Mountainkiller in Rockforge.

from Interface import Interface

inf = Interface(activator, me)

def main():
    if msg == "hello":
        inf.add_msg("*hic* *hic* *hic*", COLOR_YELLOW)
        inf.add_link("Excuse me?", dest = "excuse")

    elif msg == "excuse":
        inf.add_msg("*HIC* *hic* *HIC* *HIC* *HIC*", COLOR_YELLOW)
        inf.add_link("... bye, then.", action = "close")

main()
inf.finish()
