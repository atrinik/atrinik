## @file
## Script for Officer Bacon in Portu Baserrian.

from Atrinik import *
from Interface import Interface

inf = Interface(activator, me)

def main():
    if msg == "hello":
        inf.add_msg("Hey there, you new in town? Welcome to Portu Baserrian watch house.")
        inf.add_link("What can you tell me about Portu Baserrian?", dest = "tellportu")

    elif msg == "tellportu":
        inf.add_msg("It's an old harbour town on the isle of Berri Lur. Jarri Osoa still builds boats for the fishermen up the road there.")
        inf.add_link("Tell me about Berri Lur.", dest = "tellberri")

    elif msg == "tellberri":
        inf.add_msg("The island is large and warm, almost tropical. There are many dangers here though, so be careful when you're out exploring.")
        inf.add_link("What dangers?", dest = "dangers")

    elif msg == "dangers":
        inf.add_msg("I don't mean to scare you! But beware of the Bog of Eternal Stench, and rumour has it there's an old abandoned temple somewhere around here.")
        inf.add_link("Tell me about the Bog of Eternal Stench.", dest = "tellbog")
        inf.add_link("Tell me about the temple.", dest = "telltemple")

    elif msg == "tellbog":
        inf.add_msg("Eeew, you don't wanna go there. Legend has it if you touch the swamp, the smell will stay with you forever! If the wind is blowing the wrong way, you get the occasional waft of that stink even here.")
        inf.add_link("Forever?", dest = "bogforever")

    elif msg == "bogforever":
        inf.add_msg("Well, unless you can find the magical pool of cleansing. Legend says that will remove the stink from a cursed soul.")
        inf.add_link("Where can I find that?", dest = "poolcleansing")

    elif msg == "poolcleansing":
        inf.add_msg("I've not seen it myself, but people say it's somewhere in the forest south west of town.")

    elif msg == "telltemple":
        inf.add_msg("Yeah, I've no idea where exactly. But people say it's a dangerous place, with all sorts of goings on. Some even say it's a temple of Loki.")
        inf.add_link("Loki?", dest = "templeloki")

    elif msg == "templeloki":
        inf.add_msg("Weird, eh? Well if it is his temple, you wouldn't catch me there. I prefer law and order to unbridled chaos!")

main()
inf.send()
