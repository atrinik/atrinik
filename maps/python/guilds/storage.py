## @file
## Handles map-wide events in guild storage maps.

from Guild import Guild

other = WhoIsOther()
event_num = GetEventNumber()
guild = Guild(GetOptions())
player = activator.Controller()

## Check whether the specified object is inside a player.
## @return True if the object is inside a player, False otherwise.
def is_in_player(obj):
    while obj.env:
        obj = obj.env

    return obj.type == Type.PLAYER

def main():
    # Don't allow skills to be used, except by administrators/OPs.
    if event_num == MEVENT_SKILL_USED:
        if guild.member_is_admin(activator.name) or "[OP]" in player.cmd_permissions:
            return

        pl.DrawInfo("You can't use skills here.", COLOR_RED)
        SetReturnValue(1)
    # Allow anyone to drop items and put them to containers, but log it.
    elif event_num == MEVENT_DROP:
        # Find objects on the floor layer.
        layer = activator.map.GetLayer(activator.x, activator.y, LAYER_FLOOR)

        # There must be floor, and it must have 'unique 1' set.
        if not layer or not layer[0] or not layer[0].f_unique:
            pl.DrawInfo("You cannot drop that here; use one of the storage areas.", COLOR_RED)
            SetReturnValue(1)
            return

        guild.log_add("{} dropped {}.".format(activator.name, other.GetName()))
    elif event_num == MEVENT_PUT:
        if is_in_player(WhoAmI()):
            return

        if other.custom_name and other.custom_name[:13].lower() == "rank access: " and guild.member_is_admin(activator.name):
            rank = other.custom_name[13:]

            if not WhoAmI().f_no_pick:
                pl.DrawInfo("Only non-pickable containers can be given rank access.", COLOR_RED)
            elif rank == "None":
                pl.DrawInfo("The {} is now accessible to all.".format(WhoAmI().GetName()), COLOR_GREEN)
                WhoAmI().title = None
            elif not guild.rank_exists(rank):
                pl.DrawInfo("No such rank '{}'.".format(rank), COLOR_RED)
            else:
                pl.DrawInfo("The {} is now only accessible to members with the '{}' rank.".format(WhoAmI().GetName(), rank), COLOR_GREEN)
                WhoAmI().title = "[" + rank + "]"

            SetReturnValue(1)
            return

        guild.log_add("{} put {} into the {}.".format(activator.name, other.GetName(), WhoAmI().GetName()))
    # Picking up item, let's check if we can pick it up or not.
    elif event_num == MEVENT_PICK:
        if is_in_player(other):
            return

        if not "[OP]" in player.cmd_permissions and not guild.member_is_admin(activator.name) and not guild.member_can_pick(activator.name, other):
            pl.DrawInfo("Your rank limits you from picking up the {}. Please see the Guild Storage Manager NPC for more details.".format(other.GetName()), COLOR_BLUE)
            SetReturnValue(1)
            return

        if other.env:
            guild.log_add("{} took {} from the {}.".format(activator.name, other.GetName(), other.env.GetName()))
        else:
            guild.log_add("{} took {}.".format(activator.name, other.GetName()))
    # Don't allow applying anything other than chests.
    elif event_num == MEVENT_APPLY:
        if is_in_player(other):
            return

        if other.type != Type.CONTAINER and not other.f_no_pick:
            pl.DrawInfo("You must get it first!\n", COLOR_WHITE)
            SetReturnValue(OBJECT_METHOD_OK + 1)
        elif other.type == Type.CONTAINER and other.title and guild.member_get_rank(activator.name) != other.title[1:-1] and not guild.member_is_admin(activator.name):
            pl.DrawInfo("The {} is only accessible to those with the {} rank.".format(other.GetName(), other.title[1:-1]), COLOR_ORANGE)
            SetReturnValue(OBJECT_METHOD_OK + 1)
    elif event_num == MEVENT_CMD_DROP or event_num == MEVENT_CMD_TAKE:
        if not "[OP]" in player.cmd_permissions and not guild.member_is_admin(activator.name):
            pl.DrawInfo("You cannot use that command here.", COLOR_RED)
            SetReturnValue(1)
            return

        guild.log_add("{} used /{} {}.".format(activator.name, event_num == MEVENT_CMD_DROP and "drop" or "take", WhatIsMessage()))

main()
