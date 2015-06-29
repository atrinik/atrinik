## @file
## Used for map-wide events in Hierro's Lair.

from Atrinik import *

event_num = GetEventNumber()

def main():
    # Apply an object.
    if event_num == MEVENT_APPLY:
        other = WhoIsOther()

        # Do not allow players to apply lights.
        if activator.type == Type.PLAYER and other.type == Type.LIGHT_APPLY:
            pl.DrawInfo("Something seems to prevent you from using the {}...".format(other.GetName()), COLOR_WHITE)
            SetReturnValue(OBJECT_METHOD_OK + 1)
    # Enter the map.
    elif event_num == MEVENT_ENTER:
        # Same level as the activator.
        activator.map.difficulty = activator.level

main()
