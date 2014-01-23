## @file
## Script for Ixovolth, a mage in Fort Sether.

from Interface import Interface

inf = Interface(activator, me)

def main():
    if msg == "hello":
        inf.add_msg("Hello there. I am {} the mage, studying Promethia Island.".format(me.name))
        inf.add_link("Tell me more about this Promethia Island.", dest = "tellmore")

    elif msg == "tellmore":
        inf.add_msg("It is a small island south of Strakewood Island. The interesting part is the volcano on the island.")
        inf.add_msg("The island can be reached by taking a ship on the eastern shore of Strakewood, roughly southeast from here.")
        inf.add_link("Tell me about the volcano.", dest = "tellvolcano")

    elif msg == "tellvolcano":
        inf.add_msg("The volcano is said to be very deep, with an almost infinite depth.")

main()
inf.finish()
