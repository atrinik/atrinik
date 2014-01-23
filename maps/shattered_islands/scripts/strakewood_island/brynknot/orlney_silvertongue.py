## @file
## Script for Orlney Silvertongue atop of the Brynknot bank.

from Interface import Interface

inf = Interface(activator, me)

def main():
    if msg == "hello":
        inf.add_msg("Very impressive, adventurer! You found out about the illusionary wall, did you? Very well then.")
        inf.add_msg("I'm {}, the president for Banks Inc.".format(me.name))
        inf.add_link("Tell me about Banks Inc...", dest = "tellbanks")

    elif msg == "tellbanks":
        inf.add_msg("Our job is to provide citizens of this world a way to access their funds from anywhere, including when paying from shops or when paying for various services.")

main()
inf.finish()
