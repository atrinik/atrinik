## @file
## Script for Agnes Marne, leader of the Asterian Thieves' Guild.

from Interface import Interface

inf = Interface(activator, me)

def main():
    if msg == "hello":
        inf.add_msg("I see someone has found their way down here.  Tell me, what can my shipping company do for you?")
        inf.add_link("Shipping?", dest = "shipping")
        inf.add_link("I have some questions...", dest = "questions")
        inf.add_link("Goodbye.", action = "close")

    elif msg == "shipping":
        inf.add_msg("Sure, we ship lots of goods for the right price.")
        inf.add_link("I have some questions...", dest = "questions")
        inf.add_link("Goodbye.", action = "close")

    elif msg == "questions":
        inf.add_msg("Some questions have answers that people don't like.  Unless you have some business here, you should probably go.")
        inf.add_link("Goodbye.", action = "close")

main()
inf.finish()
