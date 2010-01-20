## @file
## This file holds common code used across Atrinik Python scripts.

## Calculate the diagonal distance between two X and Y coordinates.
def diagonal_distance(x1, y1, x2, y2):
	return max(abs(x1 - x2), abs(y1 - y2))

SIZEOFFREE1 = 8
SIZEOFFREE2 = 24
SIZEOFFREE = 49

## X offset when searching around a spot.
freearr_x = [0, 0, 1, 1, 1, 0, -1, -1, -1, 0, 1, 2, 2, 2, 2, 2, 1, 0, -1, -2, -2, -2, -2, -2, -1, 0, 1, 2, 3, 3, 3, 3, 3, 3, 3, 2, 1, 0, -1, -2, -3, -3, -3, -3, -3, -3, -3, -2, -1]
## Y offset when searching around a spot.
freearr_y = [0, -1, -1, 0, 1, 1, 1, 0, -1, -2, -2, -2, -1, 0, 1, 2, 2, 2, 2, 2, 1, 0, -1, -2, -2, -3, -3, -3, -3, -2, -1, 0, 1, 2, 3, 3, 3, 3, 3, 3, 3, 2, 1, 0, -1, -2, -3, -3, -3]
