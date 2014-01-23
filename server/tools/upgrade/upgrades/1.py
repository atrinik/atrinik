# Upgrade for changing mana and grace food to magical, and
# increasing value of all food except lesser. Also removes
# age force objects.

def upgrade_func(arch):
    if arch["archname"] in ("age_force_human", "age_force_half_elf"):
        return None

    artifact = Upgrader.arch_get_attr_val(arch, "artifact")

    if artifact:
        if artifact in ("food_mana_lesser", "food_mana_medium", "food_mana", "food_mana_greater", "food_grace_lesser", "food_grace_medium", "food_grace", "food_grace_greater"):
            i = Upgrader.arch_get_attr_num(arch, "is_magical", 1)
            arch["attrs"][i][1] = 1

            if artifact.find("_medium") != -1:
                i = Upgrader.arch_get_attr_num(arch, "value", 10)

                if arch["attrs"][i][1] < 100:
                    arch["attrs"][i][1] += 100
            elif artifact in ("food_mana", "food_grace"):
                i = Upgrader.arch_get_attr_num(arch, "value", 10)

                if arch["attrs"][i][1] < 100:
                    arch["attrs"][i][1] += 200
            elif artifact.find("_greater") != -1:
                i = Upgrader.arch_get_attr_num(arch, "value", 10)

                if arch["attrs"][i][1] < 100:
                    arch["attrs"][i][1] += 300

    return arch

upgrader = Upgrader.ObjectUpgrader(files, upgrade_func)
upgrader.upgrade()
