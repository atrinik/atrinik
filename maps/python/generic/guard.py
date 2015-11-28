"""
Generic script for guards.
"""

from Atrinik import *
from Guard import Guard


class InterfaceDialog(Guard):
    """
    Dialog when talking to the guard.
    """

if me.enemy:
    if msg == "hello":
        replies = [
            "Die, you filth!",
            "Arrghhh!!",
        ]

        if me.enemy.race == "demon":
            replies += [
                "Begone, evil spawn of the devil!",
            ]

        me.Say(random.choice(replies))
else:
    ib = InterfaceDialog(activator, me)
    ib.finish(locals(), msg)
