## @file
## Script for Princess Helena in the Asteria Library.

from Interface import Interface

inf = Interface(activator, me)

def main():
    if msg == "hello":
        inf.add_msg("Can't you see I'm reading?")
        inf.add_link("<b>You</b> are reading?", dest = "reading")
        inf.add_link("Sorry.", action = "close")

    elif msg == "reading":
        inf.add_msg("Yes. Is it a crime for the daughter of a king to enjoy reading?")

main()
inf.finish()
