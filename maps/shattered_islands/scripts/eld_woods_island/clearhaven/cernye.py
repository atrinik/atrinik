## @file
## Script for Cernye in Clearhaven.

from Interface import Interface

inf = Interface(activator, me)

def main():
    if msg == "hello":
        inf.add_msg("Good day, adventurer.")
        inf.add_msg("I'm {}, and I collect various herbs.".format(me.name))
        inf.add_link("Tell me more...", dest = "tellmore")

    elif msg == "tellmore":
        inf.add_msg("Eld Woods Island is a home to a variety of herbs, but I'm especially fond of Athalas.")
        inf.add_link("Athalas?", dest = "athalas")

    elif msg == "athalas":
        inf.add_msg("Athalas would be very useful for me, but I don't know any accessible places to find it. If you happen to find any on your travels, please tell me where you found it.")

main()
inf.finish()
