## @file
## Script for Brelgrim, a guard investigating the disappearence of Lynren
## the paladin.

from Interface import Interface

inf = Interface(activator, me)

def main():
    if msg == "hello":
        inf.add_msg("Apologies citizen, but this is a serious investigation. Please, excuse me.")
        inf.add_link("Where is Lynren?", dest = "where")

    elif msg == "where":
        inf.add_msg("Well. That is what my investigation is about. You see, she left without a word, except the explanation on the sign outside. She has been gone for a long time now, and we don't know whether she is okay or not...")

main()
inf.finish()
