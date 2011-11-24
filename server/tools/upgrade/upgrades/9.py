import random

def upgrade_func(arch):
	if arch["archname"] in ("horn", "rod_light", "rod_heavy"):
		i = Upgrader.arch_get_attr_num(arch, "maxhp", None)
		i2 = Upgrader.arch_get_attr_num(arch, "hp", None)

		if i != -1 and i2 != -1:
			arch["attrs"][i][1] = 0
			arch["attrs"][i2][1] = 0

			if arch["archname"] == "rod_light":
				arch["attrs"][i][1] = arch["attrs"][i2][1] = 3 + random.randint(0, 3)
			elif arch["archname"] == "rod_heavy":
				arch["attrs"][i][1] = arch["attrs"][i2][1] = 6 + random.randint(0, 6)

		i = Upgrader.arch_get_attr_num(arch, "speed", None)

		if i != -1:
			arch["attrs"].pop(i)

		i = Upgrader.arch_get_attr_num(arch, "artifact", None)

		if i != -1 and arch["attrs"][i][1] == "horn_normal":
			arch["attrs"].pop(i)

	return arch

upgrader = Upgrader.ObjectUpgrader(files, upgrade_func)
upgrader.upgrade()
