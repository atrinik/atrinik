## @file
## This file holds common code used across Atrinik Python scripts.

## Calculate the diagonal distance between two X and Y coordinates.
def diagonal_distance(x1, y1, x2, y2):
	return max(abs(x1 - x2), abs(y1 - y2))

## Computes an absolute direction.
## @param d Direction to convert.
## @return Number between 1 and 8, which represents the "absolute" direction
## of a number (it actually takes care of "overflow" in previous calculations
## of a direction). */
def absdir(d):
	while d < 1:
		d += 8

	while d > 8:
		d -= 8

	return d

## Returns a random direction (1..8) similar to a given direction.
## @param d The exact direction.
## @return The randomized direction. */
def get_randomized_dir(d):
	from random import randint
	return absdir(d + randint(0, 2) + randint(0, 2) - 2)

## Search squares around the activator, looking for an object identified
## by its name/count.
## @param activator The activator. Will search below and around this object.
## @param limit Maximum number of squares from the activator to search.
## @param archname Optional archname to look for.
## @param name Optional name to look for.
## @param count Optional object ID to look for.
def find_obj(activator, limit = 10, archname = None, name = None, count = None):
	if archname == None and name == None and count == None:
		raise AttributeError("No matching conditions provided.")

	for (m, x, y) in [(activator.map, activator.x, activator.y)] + activator.SquaresAround(limit):
		first = m.GetFirstObject(x, y)

		if not first:
			continue

		for tmp in first:
			if archname != None and tmp.arch.name != archname:
				continue

			if name != None and tmp.name != name:
				continue

			if count != None and tmp.count != count:
				continue

			return tmp
