def completed_tutorial(player):
    strs = [
        "savebed_map /shattered_islands/world_0303\n",
        "savebed_map /tutorial\n",
    ]

    for s in strs:
        if s in player:
            return False

    return True

def goto_emergency(player, arches, m):
    idx = player.index(m)
    player[idx] = "map /emergency\n"

    i = Upgrader.arch_get_attr_num(arches[0], "x", None)

    if i != -1:
        arches[0]["attrs"].pop(i)

    i = Upgrader.arch_get_attr_num(arches[0], "y", None)

    if i != -1:
        arches[0]["attrs"].pop(i)

    i = Upgrader.arch_get_attr_num(arches[0], "name", None)
    print("{0} was in removed area, moved to emergency.".format(arches[0]["attrs"][i][1] if i != -1 else "???"))

def player_upgrade_func(player, arches):
    if not completed_tutorial(player):
        i = Upgrader.arch_get_attr_num(arches[0], "name", None)
        print("{0} has not completed old tutorial, removing.".format(arches[0]["attrs"][i][1] if i != -1 else "???"))
        return (None, None)

    tut_maps = [
        "/shattered_islands/world_0204",
        "/shattered_islands/world_0203",
        "/shattered_islands/world_0202",
        "/shattered_islands/world_0305",
        "/shattered_islands/world_0304",
        "/shattered_islands/world_0303",
        "/shattered_islands/world_0302",
        "/shattered_islands/world_0404",
        "/shattered_islands/world_0403",
        "/shattered_islands/world_0402",
        "/shattered_islands/strakewood_island/brynknot/sewers/sewers_a_0202",
        "/shattered_islands/strakewood_island/brynknot/sewers/sewers_a_0203",
        "/shattered_islands/strakewood_island/brynknot/sewers/sewers_a_0204",
        "/shattered_islands/strakewood_island/brynknot/sewers/sewers_a_0302",
        "/shattered_islands/strakewood_island/brynknot/sewers/sewers_a_0303",
        "/shattered_islands/strakewood_island/brynknot/sewers/sewers_a_0304",
        "/shattered_islands/strakewood_island/brynknot/sewers/sewers_a_0402",
        "/shattered_islands/strakewood_island/brynknot/sewers/sewers_a_0403",
        "/shattered_islands/strakewood_island/brynknot/sewers/sewers_a_0404",
        "/shattered_islands/strakewood_island/brynknot/sewers/sewers_b_0404",
    ]

    i = Upgrader.arch_get_attr_num(arches[0], "x", None)
    player_x = arches[0]["attrs"][i][1] if i != -1 else 0

    i = Upgrader.arch_get_attr_num(arches[0], "y", None)
    player_y = arches[0]["attrs"][i][1] if i != -1 else 0

    for tut_map in tut_maps:
        s = "map " + tut_map + "\n"

        if s in player:
            goto_emergency(player, arches, s)
            break

    s = "map /shattered_islands/strakewood_island/brynknot/sewers/sewers_0101\n"

    if s in player:
        coords = [(14, 0), (15, 0), (16, 0), (17, 0), (14, 1), (15, 1), (16, 1), (17, 1)]

        for (x, y) in coords:
            if x == player_x and y == player_y:
                goto_emergency(player, arches, s)
                break

    s = "map /shattered_islands/strakewood_island/brynknot/sewers/sewers_0102\n"

    if s in player:
        coords = [(10, 15), (10, 16), (10, 18), (10, 19), (10, 20), (11, 14), (11, 15), (11, 16), (11, 17), (11, 18), (11, 19), (11, 20), (12, 15), (12, 16), (12, 18), (12, 19), (12, 20), (13, 19), (14, 14), (14, 15), (14, 16), (14, 17), (14, 18), (14, 19), (14, 20), (14, 21), (14, 22), (14, 23), (15, 14), (15, 15), (15, 16), (15, 17), (15, 18), (15, 19), (15, 20), (15, 21), (15, 22), (15, 23), (16, 14), (16, 15), (16, 16), (16, 18), (16, 20), (16, 22), (17, 14), (17, 15), (17, 16), (17, 17), (17, 18), (17, 19), (17, 20), (17, 21), (17, 22), (17, 23)]

        for (x, y) in coords:
            if x == player_x and y == player_y:
                goto_emergency(player, arches, s)
                break

    s = "map /shattered_islands/strakewood_island/brynknot/sewers/sewers_0202\n"

    if s in player:
        coords = [(17, 9), (18, 9), (17, 10), (18, 10), (18, 11)]

        for (x, y) in coords:
            if x == player_x and y == player_y:
                goto_emergency(player, arches, s)
                break

    return (player, arches)

upgrader = Upgrader.ObjectUpgrader(files)
upgrader.set_player_upgrade_func(player_upgrade_func)
upgrader.upgrade()
