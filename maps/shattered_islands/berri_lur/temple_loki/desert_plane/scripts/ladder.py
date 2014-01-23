## @file
## Script for the ladder in Loki's Desert Plane. When applied, it will
## jump away from the activator, so they cannot climb it down. The trick
## is to get it next to a wall, where it can't escape.

from Common import get_randomized_dir
import os.path

def main():
    direction = get_randomized_dir(activator.direction)
    # Calculate new X/Y
    x = me.x + freearr_x[direction]
    y = me.y + freearr_y[direction]

    # If there's no wall, continue going away from the activator.
    if not me.map.Wall(x, y):
        (m, x, y) = me.map.GetMapFromCoord(x, y)
        m.Insert(me, x, y)
        SetReturnValue(1)
    # Can't go further; put it back where it came from, and allow activator
    # to climb it down.
    else:
        m = ReadyMap(os.path.dirname(me.map.path) + "/desert_plane_0101")
        m.Insert(me, 23, 0)
