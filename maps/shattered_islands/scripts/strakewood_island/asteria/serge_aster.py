## @file
## Script for Serge Aster, the thief.

from Interface import Interface

inf = Interface(activator, me)

def main():
    if msg == "hello":
        inf.add_msg("Oi.  What are you doing down here?")
        inf.add_link("I have some questions...", dest = "questions")
        inf.add_link("Goodbye.", action = "close")

    elif msg == "questions":
        inf.add_msg("Some questions are dangerous, friend.  Unless you have some business here, you should probably go.")
        inf.add_link("Goodbye.", action = "close")

main()
inf.finish()
