## @file
## Script for Garland in Brynknot.

from Interface import Interface

inf = Interface(activator, me)

def main():
    if msg == "hello":
        inf.add_msg("Why, hello there, my name is {}. I am making a drawing of the portal for my journal.".format(me.name))
        inf.add_link("Tell me more...", dest = "tellmore")

    elif msg == "tellmore":
        inf.add_msg("I have some history about the portal in it, take a look if you like.")

main()
inf.finish()
