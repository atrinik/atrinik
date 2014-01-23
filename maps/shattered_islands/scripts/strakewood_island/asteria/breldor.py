## @file
## Breldor the fisherman in Asteria, east of the Business Plaza.

from Interface import Interface

inf = Interface(activator, me)

def main():
    if msg == "hello":
        inf.add_msg("Hello! I am {} the fisherman. I am currently taking a break, but I reckon I should get back to work soon...".format(me.name))
        inf.add_link("What work?", dest = "work")

    elif msg == "work":
        inf.add_msg("I supply Asteria citizens with fresh fish, so I have a lot of work to do!")

main()
inf.finish()
