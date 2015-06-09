## @file
## Script to be executed when Hierro dies.

# X/Y locations of various objects to remove/adjust when Hierro dies.
locations = [
    # No need for the map event objects now.
    (0, 25), (0, 26),
    # Torches to activate all at once.
    (8, 20), (8, 16), (8, 12), (8, 8), (8, 4), (15, 20), (15, 16), (15, 12), (15, 8), (15, 4),
    # Magic mouths to remove.
    (9, 5), (10, 5), (11, 5),
    # Door to unlock.
    (10, 4),
    # May as well remove the markers that were used to allow player
    # to open the above door.
    (9, 2), (10, 2), (11, 2), (12, 2), (10, 3), (11, 3),
    # Adjust the exit-message for the stairs leading out of the dungeon.
    (8, 25), (9, 25), (10, 25),
]

if activator.owner:
    activator = activator.owner

def main():
    me.map.DrawInfo(me.x, me.y, "You hear a booming voice...", color = COLOR_GREEN, distance = 20)
    me.map.DrawInfo(me.x, me.y, "Noooooooooooooooo! Curse you {}, curse yoouuu!...".format(activator.race), color = COLOR_RED, distance = 20)

    # Remove the spawn point... no more Hierro for this player.
    me.FindObject(type = Type.SPAWN_POINT_INFO).owner.Destroy()

    for (x, y) in locations:
        for obj in me.map.GetFirstObject(x, y):
            # Remove markers, inventory checkers and signs (magic mouths).
            if obj.type in (Type.MARKER, Type.CHECK_INV, Type.SIGN):
                obj.Destroy()
            # Unlock doors.
            elif obj.type == Type.DOOR:
                obj.slaying = None
            # Change torches back to normal and light them up.
            elif obj.type == Type.LIGHT_APPLY:
                if obj.f_no_pick:
                    obj.Apply(obj, APPLY_NORMAL)
            # Remove the map event objects.
            elif obj.type == Type.MAP_EVENT_OBJ:
                obj.Destroy()
            # Adjust stairs message.
            elif obj.type == Type.EXIT:
                obj.msg = "As you go up the stairs to the surface, there is a great rumble, and rocks fall from the ceiling, blocking the passage!\nIt seems you won't be able to go back..."

main()
