## @file
## Script for Cathedral Registration attendants in the Catherdral of Holy Wisdom.

from Interface import Interface

inf = Interface(activator, me)

def main():
    if msg == "hello":
        inf.add_msg("Good day.  Would you like to go in?")
        inf.add_link("Yes.", dest = "entry")
        inf.add_link("Never mind.  Bye.", action = "close")

    elif msg == "entry":
        inf.add_msg("Enter in peace and behold the great and powerful majesty of the Tabernacle.")

main()
inf.finish()
