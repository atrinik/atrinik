## @file
## Script for gardener in houses, to go around the house collecting
## pickable items outside the house and putting them in front of the
## house.

from Atrinik import *
from House import *
import random

activator = WhoIsActivator()
house = House(activator, GetOptions())
r = random.random()

# To save a bit of CPU, only call this sometimes (makes it look like
# the gardener overlooks items occasionally).
if r <= 0.30:
	(new_x, new_y) = house.get(house.gardener_pos)

	# Find squares around the gardener that are not blocked by wall.
	for t in activator.SquaresAround(house.get(house.gardener_around), AROUND_WALL | AROUND_PLAYER_ONLY, True) + [(activator.map, activator.x, activator.y)]:
		(m, x, y) = t

		# Ignore the square where the items go.
		if x == new_x and y == new_y:
			continue

		# Go through the objects on this square.
		for ob in m.GetFirstObject(x, y):
			# Ignore non-pickable and system objects.
			if ob.f_sys_object or ob.f_no_pick or not ob.weight:
				continue

			# It's an object that we can pick up, so move it.
			m.Insert(ob, new_x, new_y)
