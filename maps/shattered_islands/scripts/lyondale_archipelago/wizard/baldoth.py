## @file
## Script for Baldoth at the Wizards' Tower.

from Interface import Interface

inf = Interface(activator, me)

def main():
    if msg == "hello":
        inf.add_msg("Welcome to the Wizards' Tower island in Lyondale Archipelago.")
        inf.add_msg("If you are seeking the knowledge about Thelras or magic, I recommend you go see Teldach inside the tower.")
        inf.add_link("Tell me about Thelras.", dest = "tellthelras")
        inf.add_link("Who is Teldach?", dest = "whoteldach")

    elif msg == "tellthelras":
        inf.add_msg("We Thelras are heavy users of magic. Spellcasting, wands, potions, horns, things like that. Our magic abilities are far greater than that of any other race.")
        inf.add_msg("For more information, go see Teldach to learn about more about Thelras.")
        inf.add_link("Who is Teldach?", dest = "whoteldach")

    elif msg == "whoteldach":
        inf.add_msg("Teldach is the oldest of our race. He's just inside the tower; you should go see him.")

main()
inf.finish()
