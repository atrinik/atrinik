## @file
## Generic script for jail guards.

from Atrinik import *
from Interface import Interface
import Jail

inf = Interface(activator, me)

def main():
    time_left = Jail.get_jail_time(activator)

    if time_left:
        if msg == "hello":
            inf.add_msg("You need to wait {} before being released, {}.".format(time_left, activator.name))
            inf.add_link("I'm sorry, can I have some food?", dest = "sorryfood")
            inf.add_link("Okay...", action = "close")

        elif msg == "sorryfood":
            if activator.food >= 300:
                inf.add_msg("You don't look very hungry to me. I'll have some food ready for you when you do get hungry, though.")
            else:
                inf.add_msg("Here you go...")
                objs = [CreateObject("bread"), CreateObject("cheese"), CreateObject("drink_generic")]
                inf.add_objects(objs)

                for obj in objs:
                    activator.Apply(obj)
    else:
        inf.add_msg("Move along. We'll have no trouble here.")

main()
inf.send()
