## @file
## Script for Galsteen, the Aris <-> Brynknot tunnel guard captain.

from Interface import Interface

inf = Interface(activator, me)

def main():
    if msg == "hello":
        inf.add_msg("Greetings adventurer. I'm {}, the captain of this outpost.".format(me.name))
        inf.add_msg("Well, not really much of an outpost to be honest. We're here guarding this tunnel that connects Brynknot and Aris.")
        inf.add_link("Why is there no bridge?", dest = "bridge")

    elif msg == "bridge":
        inf.add_msg("We first tried building a bridge further east, but we have been unsuccessful because of the swift current. We didn't get much progress done, apart from some unstable foundation, which we have been unable to finish.")
        inf.add_msg("If you're agile enough I suppose you could try using the foundation platforms to get from one side to the other. However, be careful, the current often gets stronger than usual, and it could catch you unaware and send you drowning into the river!")
        inf.add_msg("So instead we built a tunnel here, after determining that the current is too swift even here.")
        inf.add_link("How safe is the tunnel?", dest = "safe")

    elif msg == "safe":
        inf.add_msg("Very safe! We are closely guarding it so no monsters can pass or take the tunnel as their own. Of course, there's the issue of some spider infestation. No one has seen the spiders so we've been unable to get rid of them, but the cobwebs just keep appearing after we remove them!")

main()
inf.finish()
