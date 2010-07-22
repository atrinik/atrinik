## @file
## Script for item destroyers in public house areas.

from Atrinik import *
import random

activator = WhoIsActivator()

# Whatever is triggering the destroyer must have an owner and the owner's
# name must be 'magic wall'.
if activator.owner and activator.owner.name == "magic wall":
	# Go through objects on activator's square.
	for ob in activator.map.GetFirstObject(activator.x, activator.y):
		if ob.f_sys_object or ob.f_no_pick or not ob.weight or ob.type == TYPE_PLAYER:
			continue

		# Randomly calculate when to destroy an item.
		if random.random() <= 0.10:
			activator.map.Message(activator.x, activator.y, 5, "The {0} destroys the {1}!".format(activator.name, ob.GetName()), COLOR_RED)
			ob.Remove()

SetReturnValue(1)
