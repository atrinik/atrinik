## @file
## Script for Teldach at the Wizards' Tower.

from Interface import Interface

inf = Interface(activator, me)

def main():
    if msg == "hello":
        inf.add_msg("Welcome to the Wizards' Tower. This island is part of Lyondale Archipelago, mostly inhabited by Thelras.")
        inf.add_link("Tell me about Thelras.", dest = "tellthelras")

    elif msg == "tellthelras":
        inf.add_msg("Thelras are a powerful, tall and fierce race. They excel in magic, where they can overpower even the strongest elvish spellcasters.")
        inf.add_msg("Long ago, we Thelras decided to help the Thraal army in the fight against Moroch.")
        inf.add_msg("Since most Thelras excel in magic as wizards, this tower was built for those who wanted to practice in the arts of magic.")
        inf.add_link("Tell me about the fight.", dest = "tellfight")
        inf.add_link("Tell me about the tower.", dest = "telltower")

    elif msg == "tellfight":
        inf.add_msg("We fought - and still do - together against Moroch, and became friends with the other races.")
        inf.add_msg("Many humans, dwarves and other races come to this tower to learn about magic. Of course, when we first met the dwarves, we called them 'shorties', which made many of them furious. We eventually became friends with them, though.")

    elif msg == "telltower":
        inf.add_msg("In this tower you can find various things that could help you in using your magical skills, including shops with magic tools, spells, potions, and so on.")

main()
inf.finish()
