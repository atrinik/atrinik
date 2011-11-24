def upgrade_func(arch):
	i = Upgrader.arch_get_attr_num(arch, "artifact", None)

	if i != -1:
		artname = arch["attrs"][i][1]
		arch["attrs"].pop(i)

		if artname.startswith("ring_") or artname.startswith("amulet_"):
			arch["archname"] = artname

			if artname.endswith("_mithril") or artname.endswith("_adamant") or artname.endswith("_platinum") or artname.endswith("_gold") or artname.endswith("_silver") or artname.endswith("_bronze") or artname.endswith("_copper") or artname.endswith("_brass"):
				pass
			else:
				i = Upgrader.arch_get_attr_num(arch, "value", None)

				if i != -1:
					arch["attrs"].pop(i)

	return arch

upgrader = Upgrader.ObjectUpgrader(files, upgrade_func)
upgrader.upgrade()
