# Upgrade for changing the 'wizard' race to 'thelra'.

def upgrade_func(arch):
    if arch["archname"] == "wizard_male":
        arch["archname"] = "thelra_male"
    elif arch["archname"] == "wizard_female":
        arch["archname"] = "thelra_female"

    i = Upgrader.arch_get_attr_num(arch, "animation")

    if i != -1:
        if arch["attrs"][i][1] == "wizard_female":
            arch["attrs"][i][1] = "thelra_female"
        elif arch["attrs"][i][1] == "wizard_male":
            arch["attrs"][i][1] = "thelra_male"

    i = Upgrader.arch_get_attr_num(arch, "race")

    if i != -1:
        if arch["attrs"][i][1] == "wizard":
            arch["attrs"][i][1] = "thelra"

    return arch

upgrader = Upgrader.ObjectUpgrader(files, upgrade_func)
upgrader.upgrade()
