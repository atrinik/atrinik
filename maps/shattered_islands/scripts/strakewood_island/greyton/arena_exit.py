## @file
## Script to teleport a player that has died to map with "local
## medics" randomly to one of the beds.

from random import choice

# Randomly choose the X/Y.
activator.TeleportTo("/shattered_islands/world_1113", choice((7, 10, 13, 16)), choice((7, 8)))
