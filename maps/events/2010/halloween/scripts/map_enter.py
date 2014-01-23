## @file
## Script ran when player enters the abyss map.
##
## Adjusts the map difficulty and sets magic wall levels.

from Atrinik import *

activator = WhoIsActivator()
# Same level as the activator.
activator.map.difficulty = activator.level

# Adjust the magic walls.
for (x, y) in [(29, 6), (29, 14), (37, 6), (37, 14)]:
    for ob in activator.map.GetLayer(x, y, LAYER_SYS):
        if ob.name == "magic wall":
            ob.level = activator.level
            break
