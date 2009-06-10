# Script to teleport a player that has died to map with "local medics" randomly to one of the beds.

import Atrinik
import random

ac = Atrinik.WhoIsActivator()

# Random integer between 0 and 4.
rand = random.randint(0, 4)

# y will be 4 + rand, x will be 6
y = 4 + rand
x = 6

# If y is an odd number (5, 7), add 1 (6, 8). Also add 1 to x in this case.
if y % 2:
	y += 1
	x += 1

ac.TeleportTo("/lost_worlds/wilderness_0061", x, y, 0)
