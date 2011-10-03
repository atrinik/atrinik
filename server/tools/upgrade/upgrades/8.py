def upgrade_func(arch):
	if arch["archname"] == "quest_container":
		i = Upgrader.arch_get_attr_num(arch, "name", None)

		if i != -1:
			if arch["attrs"][i][1] in ("Shipment of Charob Beer", "Ogres near Brynknot", "Evil Trees at Brynknot", "Forgotten Graveyard at Strakewood Island", "Giant Wasps at Strakewood Island", "Wyverns at Strakewood Island", "Ice Golems in Old Outpost", "Lava Golems in Old Outpost", "Manard's Prayerbook", "Cashin's Cap", "Tutorial Island Well", "Slimes at Tutorial Island", "Frah'ak and Kobolds", "Jahrlen's Heavy Rod", "Hill Giants Stronghold", "Dark Cave Elder Wyverns", "Enemies beneath Brynknot", "Investigation for Maplevale", "Aris Undead Infestation", "Ants under Fort Ghzal"):
				arch["archname"] = None

		i = Upgrader.arch_get_attr_num(arch, "underground_city_lake_portal", None)

		if i != -1:
			arch["attrs"].pop(i)
	elif arch["archname"] == "barrel2.101":
		i = Upgrader.arch_get_attr_num(arch, "name", None)

		if i != -1:
			if arch["attrs"][i][1] == "shipment of Charob Beer":
				arch["archname"] = None
	elif arch["archname"] == "key2":
		i = Upgrader.arch_get_attr_num(arch, "name", None)

		if i != -1:
			if arch["attrs"][i][1] in ("Maplevale's amulet", "Brynknot Maze Key"):
				arch["archname"] = None
	elif arch["archname"] == "note":
		i = Upgrader.arch_get_attr_num(arch, "name", None)

		if i != -1:
			if arch["attrs"][i][1] == "Letter from Nyhelobo to oty captain":
				arch["archname"] = None

	return arch

upgrader = Upgrader.ObjectUpgrader(files, upgrade_func)
upgrader.upgrade()
