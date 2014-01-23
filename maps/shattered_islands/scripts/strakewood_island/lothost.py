## @file
## Script for Lothost the wizard, south of the Abandoned Mine main
## entrance.

from Interface import Interface

inf = Interface(activator, me)

def main():
    if msg == "hello":
        inf.add_msg("Hello there. I'm {} the wizard. I'm currently studying the abandoned mine north of here.".format(me.name))
        inf.add_link("Tell me more...", dest = "tellmore")

    elif msg == "tellmore":
        inf.add_msg("It's an abandoned dwarven mine that makes some sort of a tunnel between several locations. Rumor is that a portal to the legendary Moroch Temple is located there, which is what I'm trying to find out about.")
        inf.add_msg("If you are also interested in the mine, you can read about it in my notes in the bookcase.")

main()
inf.finish()
