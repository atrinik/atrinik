## @file
## Script for the Hierro's Lair stairs entrance.

from Atrinik import *

def main():
    if GetTime()["season_name"] != "Season of the Blizzard" or activator.Controller().quest_container.FindObject(name = "hierro_ring"):
        pl.DrawInfo("You go down the stairs, but after a couple steps find your passage blocked by rocks. As there doesn't seem to be any way to the cave, you return.", COLOR_YELLOW)
        SetReturnValue(1)
    else:
        extinguished = 0

        # Find applyable lights in player's inventory and extinguish
        # them all.
        for obj in activator.FindObjects(mode = INVENTORY_CONTAINERS, type = Type.LIGHT_APPLY):
            # Is the light actually lit?
            if obj.glow_radius:
                extinguished += 1
                # Extinguish it.
                obj.Apply(obj, APPLY_NORMAL)

                # If it was not applied, only lit, it will be applied
                # now, so unapply it.
                if obj.f_applied:
                    obj.Apply(obj, APPLY_NORMAL)

        if extinguished:
            from Language import pluralize
            pl.DrawInfo("Some strange power has extinguished the {} you are carrying...".format(pluralize("light", extinguished)), COLOR_YELLOW)

main()
