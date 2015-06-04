## @file
## Blocks object from randomly moving onto all squares which have floor,
## except those that match regex specified in event options (matched
## against the floor's arch name).

from Atrinik import *
import re

options = GetOptions()
# Direction the object wants to move into
d = GetEventParameters()[1]

try:
    # Get the floor on the square the object wants to move into.
    floor = me.map.GetLayer(me.x + freearr_x[d], me.y + freearr_y[d], LAYER_FLOOR)[0]

    if not re.match(options, floor.arch.name):
        SetReturnValue(1)
except:
    pass
