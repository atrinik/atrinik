## @file
## Jail manager NPC.

from Atrinik import *
from Interface import Interface
import Jail

inf = Interface(activator, me)

def main():
    if msg == "hello":
        inf.add_msg("Hello there, {}. I am the jail manager, and I can assist you in hunting down criminals and putting them to this jail.".format(activator.name))
        inf.add_link("I'd like to jail a criminal.", dest = "jail1")
        inf.add_link("I'd like to release a criminal from the jail.", dest = "release1")

    elif msg == "jail1":
        inf.add_msg("Please type the criminal's name that you want to jail.")
        inf.set_text_input(prepend = "jail2 ")

    elif msg.startswith("jail2 "):
        name = msg[6:]
        pl = FindPlayer(name)

        if not pl:
            inf.add_msg("Hm, I don't know about this criminal. Are you sure you typed their name correctly?")
            inf.set_text_input(prepend = "jail2 ")
            return

        inf.add_msg("How long do you want to jail [green]{}[/green] for?".format(pl.name))
        inf.add_msg("For example, type [green]1 hour[/green] or [green]10 minutes and 15 seconds[/green], etc.")
        inf.set_text_input(prepend = "jail3 \"" + pl.name + "\" ")

    elif msg.startswith("jail3 "):
        import re

        match = re.match("\"([^\"]*)\" (.*)", msg[6:])

        if not match:
            inf.add_msg("Sorry, I didn't quite catch that one.")
            return

        (name, timeamount) = match.groups()

        from Language import time2seconds
        seconds = time2seconds(timeamount)
        pl = FindPlayer(name)

        if not pl:
            inf.add_msg("Hm, I don't know about this criminal.")
            return

        if not seconds:
            inf.add_msg("You need to enter a valid amount of time to jail [green]{}[/green] for.".format(pl.name))
            inf.set_text_input(prepend = "jail3 \"" + pl.name + "\" ")
            return
        elif seconds > 30000:
            inf.add_msg("That's a ridiculous amount of time to jail [green]{}[/green] for! Try again.".format(pl.name))
            inf.set_text_input(prepend = "jail3 \"" + pl.name + "\" ")
            return

        jail = Jail.Jail(me)

        if jail.jail(pl, seconds):
            inf.add_msg("{} has been jailed successfully.".format(pl.name))
        else:
            inf.add_msg("{} is already in jail.".format(pl.name))

    elif msg == "release1":
        inf.add_msg("Please type the criminal's name that you want to release.")
        inf.set_text_input(prepend = "release2 ")

    elif msg.startswith("release2 "):
        name = msg[9:]
        pl = FindPlayer(name)

        if not pl:
            inf.add_msg("Hm, I don't know about this criminal. Are you sure you typed their name correctly?")
            inf.set_text_input(prepend = "release2 ")
            return

        force = Jail.get_jail_force(pl)

        if not force:
            inf.add_msg("{} is not in jail... yet.".format(pl.name))
        else:
            force.Destroy()
            inf.add_msg("{} has been released early.".format(pl.name))
            pl.Controller().DrawInfo("You have been released early.", COLOR_GREEN)

main()
inf.send()
