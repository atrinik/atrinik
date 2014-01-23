## @file
## Script for Redhelm in Brynknot.

from Interface import Interface

inf = Interface(activator, me)

def main():
    if msg == "hello":
        inf.add_msg("Well, hello there! I'm a traveller looking for interesting artifacts and treasures. Have you seen the leftovers of a shop in the Brynknot sewers?")
        inf.add_link("Yes.", dest = "shop")
        inf.add_link("Yeah, sure.", dest = "shop")
        inf.add_link("Not yet...", dest = "shop")

    elif msg == "shop":
        inf.add_msg("Well, the shop has once been active, with various treasures being sold there for high prices, but now there's not much left of it. I used to buy stuff there and venture down the sewers you know..")
        inf.add_link("Tell me about the sewers.", dest = "sewers")

    elif msg == "sewers":
        inf.add_msg("Yes, use any of the sewer holes around the town to enter the Brynknot sewer system. It's bigger than it looks and sometimes you need to push your way through. One time I was attacked by a terrible monster!")
        inf.add_link("Tell me more...", dest = "tellmore")

    elif msg == "tellmore":
        inf.add_msg("You see this scar? That's not all it did to me, it also ate my magical pouch filled with gems and jewels!")
        inf.add_link("A magical pouch?", dest = "magicalpouch")

    elif msg == "magicalpouch":
        inf.add_msg("Yes, a magical pouch with my costly collection of gems and jewels. If you ever happen to find it, do as you please and keep it all or sell the jewels and gems to a regular shop.")

main()
inf.finish()
