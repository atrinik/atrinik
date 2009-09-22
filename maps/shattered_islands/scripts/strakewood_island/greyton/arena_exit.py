# Script to teleport a player that has died to map with "local medics" randomly to one of the beds.

from Atrinik import *
import random

activator = WhoIsActivator()

# Map path of the local medics
mapPath = "/shattered_islands/world_1113"

# Starting x and y positions (first bed)
x = 7;
y = 7;

# Loop 3 times, 1/2 chance to add 3 to x position each loop
for i in range(3):
	if (random.randint(1, 2) % 2):
		x += 3

# 1/2 chance to add 1 to y position
if (random.randint(1, 2) % 2):
	y += 1

# Teleport the activator now
activator.TeleportTo(mapPath, x, y, 0)
