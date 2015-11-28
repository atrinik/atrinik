## @file
## Script for the Old Treant in the Desert Plane of Loki's Temple.

from Atrinik import *
from Interface import Interface

inf = Interface(activator, me)

def main():
    if msg == "hello":
        inf.add_msg("I appear to be stood on your way out. Oh dear. I guess you'll have to find some way to move me.")
        inf.add_link("Er... how?", dest = "how")

    elif msg == "how":
        inf.add_msg("Have you tried pushing?")

main()
inf.send()
