## @file
## This file holds common code used across Atrinik Python scripts.

import random

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
	return absdir(d + random.randint(0, 2) + random.randint(0, 2) - 2);

SIZEOFFREE1 = 8
SIZEOFFREE2 = 24
SIZEOFFREE = 49

## X offset when searching around a spot.
freearr_x = [0, 0, 1, 1, 1, 0, -1, -1, -1, 0, 1, 2, 2, 2, 2, 2, 1, 0, -1, -2, -2, -2, -2, -2, -1, 0, 1, 2, 3, 3, 3, 3, 3, 3, 3, 2, 1, 0, -1, -2, -3, -3, -3, -3, -3, -3, -3, -2, -1]
## Y offset when searching around a spot.
freearr_y = [0, -1, -1, 0, 1, 1, 1, 0, -1, -2, -2, -2, -1, 0, 1, 2, 2, 2, 2, 2, 1, 0, -1, -2, -2, -3, -3, -3, -3, -2, -1, 0, 1, 2, 3, 3, 3, 3, 3, 3, 3, 2, 1, 0, -1, -2, -3, -3, -3]
