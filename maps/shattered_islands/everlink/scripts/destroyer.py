## @file
## Script for item destroyers in public house areas.

import random

from Atrinik import *


def main():
    # Go through objects on activator's square.
    for ob in activator.map.GetFirstObject(activator.x, activator.y):
        if ob.f_sys_object or ob.f_no_pick or not ob.weight or ob.type == Type.PLAYER:
            continue

        # Randomly calculate when to destroy an item.
        if random.random() <= 0.10:
            activator.map.Message(activator.x, activator.y, 5, "The {} destroys the {}!".format(activator.name, ob.GetName()), COLOR_RED)
            ob.Destroy()

# Whatever is triggering the destroyer must have an owner and the owner's
# name must be 'magic wall'.
if activator.owner and activator.owner.name == "magic wall":
    main()

SetReturnValue(1)
