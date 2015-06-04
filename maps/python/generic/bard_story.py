## @file
## Implements bards that tell stories from books in the bard's inventory.

from Atrinik import *
from Interface import Interface

inf = Interface(activator, me)

## Handle the NPC.
def main():
    if msg == "hello":
        inf.add_msg("Greetings {}, I am {}, the bard.".format(activator.name, me.name))
        inf.add_msg("If you like, I can perform the following plays for you.")

        # Construct a list of the possible plays.
        plays = [(obj.name, obj.arch.name) for obj in me.inv if obj.type == Type.BOOK]

        # Add the links.
        for (name, archname) in sorted(plays, key = lambda play: play[1]):
            inf.add_link(name, dest = archname)

    # See if the message matched one the possible plays in the bard's inventory.
    else:
        for obj in me.inv:
            if obj.type == Type.BOOK and obj.arch.name == msg:
                inf.add_msg("[title]" + obj.name + "[/title]")
                inf.add_msg(obj.msg)
                break

main()
inf.finish()
