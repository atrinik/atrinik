## @file
## Script for Derwent the snake priest in Loki's Temple.

from Interface import Interface

inf = Interface(activator, me)

def main():
    if msg == "hello":
        inf.add_msg("Welcome worshipper! Pray with caution, for Loki can be very unpredictable!")
        inf.add_link("Who is Loki?", dest = "aboutloki")

    elif msg == "aboutloki":
        inf.add_msg("Loki is regarded as a god of chaos, due to his playful, unpredictable nature. He is by nature however, a shapeshifter.")

main()
inf.send()
