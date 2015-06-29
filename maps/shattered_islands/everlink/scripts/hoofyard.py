## @file
## Script for Hoofyard in Everlink, who explains about the construction
## skill.

from Interface import Interface

inf = Interface(activator, me)

def main():
    if msg == "hello":
        inf.add_msg("You can buy materials in the shop around here in Everlink to use with your construction skill.")
        inf.add_msg("Do you need information about the construction skill?")
        inf.add_msg("Sure...", dest = "information")

    elif msg == "information":
        inf.add_msg("To use the construction skill, you need to buy a construction builder and a construction destroyer. With the destroyer, you can destroy previously built objects or walls. You will need a destroyer to destroy the extra walls in your house (in Greyton house this is the eastern wall, for example). Use the builder to build walls, windows, fireplaces, and so on. To use either the destroyer or the builder, you must first apply it, and then type [green]/use_skill construction[/green] or find the construction skill in your skill list, press enter and use CTRL + direction.")
        inf.add_msg("Tell me about building.", dest = "building")

    elif msg == "information about materials":
        inf.add_msg("To build using the construction builder, you need to mark a material you want to build. For example, an altar material, desk material, or wall material. Sign materials exist too, but to build them, you need a book on the square where you want to build the sign with the message you want the sign to have (custom name of the book will be copied to the sign). Windows, pictures, flags and so on can only be built on top of walls.")

main()
inf.send()
