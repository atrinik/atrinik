## @file
## Script for Morg'eean the kobold trader south of Asteria.

from Interface import Interface

inf = Interface(activator, me)

def main():
    if msg == "hello":
        inf.add_msg("Welcome! I am Morg'eean the kobold trader. I trade various goods between Asteria and Brynknot using my ship.")
        inf.add_msg("I also have a small shop in my cellar.")
        inf.add_link("Can I use your ship?", dest = "useship")
        inf.add_link("Tell me about your shop.", dest = "aboutshop")

    elif msg == "useship":
        inf.add_msg("Hmm... well, the business is slow right now, so I don't need it at the moment. Feel free to use it to sail to Brynknot.")
        inf.add_msg("You can find a spare key in my chest with goods which will open the gate. Don't worry, the ship has an enchantment! It can travel between Brynknot and Asteria by itself, no steering necessary. I can also recall it if need be! That was a costly enchantment, though...")

    elif msg == "aboutshop":
        inf.add_msg("It's just below this house, down the stairs. Feel free to have a look around!")
        inf.add_msg("I'll be especially pleased if you buy some of my goods!")

main()
inf.finish()
