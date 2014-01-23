# Upgrade for adding item_power to artifacts.

def upgrade_func(arch):
    artifact = Upgrader.arch_get_attr_val(arch, "artifact")

    if not artifact or Upgrader.arch_get_attr_val(arch, "item_power"):
        return arch

    if artifact == "sling_accuracy":
        Upgrader.arch_get_attr_num(arch, "item_power", 3)
    elif artifact == "crossbow_accuracy":
        Upgrader.arch_get_attr_num(arch, "item_power", 3)
    elif artifact == "range_accuracy":
        Upgrader.arch_get_attr_num(arch, "item_power", 3)
    elif artifact == "amulet_calling_death":
        Upgrader.arch_get_attr_num(arch, "item_power", 1)
    elif artifact == "amulet_sorrow":
        Upgrader.arch_get_attr_num(arch, "item_power", 1)
    elif artifact == "amulet_shielding":
        Upgrader.arch_get_attr_num(arch, "item_power", 8)
    elif artifact == "ring_woe3":
        Upgrader.arch_get_attr_num(arch, "item_power", 1)
    elif artifact == "ring_woe2":
        Upgrader.arch_get_attr_num(arch, "item_power", 1)
    elif artifact == "ring_woe1":
        Upgrader.arch_get_attr_num(arch, "item_power", 1)
    elif artifact == "ring_doom":
        Upgrader.arch_get_attr_num(arch, "item_power", 1)
    elif artifact == "ring_woe":
        Upgrader.arch_get_attr_num(arch, "item_power", 1)
    elif artifact == "ring_benevolence":
        Upgrader.arch_get_attr_num(arch, "item_power", 20)
    elif artifact == "ring_prelate":
        Upgrader.arch_get_attr_num(arch, "item_power", 14)
    elif artifact == "ring_paladin":
        Upgrader.arch_get_attr_num(arch, "item_power", 7)
    elif artifact == "ring_healer":
        Upgrader.arch_get_attr_num(arch, "item_power", 15)
    elif artifact == "ring_high_magic":
        Upgrader.arch_get_attr_num(arch, "item_power", 20)
    elif artifact == "ring_ancient_magic":
        Upgrader.arch_get_attr_num(arch, "item_power", 14)
    elif artifact == "ring_magic":
        Upgrader.arch_get_attr_num(arch, "item_power", 7)
    elif artifact == "ring_strife":
        Upgrader.arch_get_attr_num(arch, "item_power", 14)
    elif artifact == "ring_combat":
        Upgrader.arch_get_attr_num(arch, "item_power", 7)
    elif artifact == "ring_ghost":
        Upgrader.arch_get_attr_num(arch, "item_power", 10)
    elif artifact == "ring_yordan":
        Upgrader.arch_get_attr_num(arch, "item_power", 20)
    elif artifact == "ring_ice_great":
        Upgrader.arch_get_attr_num(arch, "item_power", 13)
    elif artifact == "ring_fire_great":
        Upgrader.arch_get_attr_num(arch, "item_power", 13)
    elif artifact == "ring_storm_great":
        Upgrader.arch_get_attr_num(arch, "item_power", 13)
    elif artifact == "ring_storm":
        Upgrader.arch_get_attr_num(arch, "item_power", 7)
    elif artifact == "ring_fire":
        Upgrader.arch_get_attr_num(arch, "item_power", 7)
    elif artifact == "ring_ice":
        Upgrader.arch_get_attr_num(arch, "item_power", 7)
    elif artifact == "ring_thieves":
        Upgrader.arch_get_attr_num(arch, "item_power", 10)
    elif artifact == "shield_holy_light":
        Upgrader.arch_get_attr_num(arch, "item_power", 30)
    elif artifact == "boots_kriabe":
        Upgrader.arch_get_attr_num(arch, "item_power", 10)
    elif artifact == "light_sandals":
        Upgrader.arch_get_attr_num(arch, "item_power", 3)
    elif artifact == "amulet_life_saving":
        Upgrader.arch_get_attr_num(arch, "item_power", 5)
    elif artifact == "gloves_drula":
        Upgrader.arch_get_attr_num(arch, "item_power", 5)
    elif artifact == "armour_skel_lord":
        Upgrader.arch_get_attr_num(arch, "item_power", 15)
    elif artifact == "cloak_rhun":
        Upgrader.arch_get_attr_num(arch, "item_power", 5)
    elif artifact == "weapon_less_elec":
        Upgrader.arch_get_attr_num(arch, "item_power", 3)
    elif artifact == "weapon_less_cold":
        Upgrader.arch_get_attr_num(arch, "item_power", 3)
    elif artifact == "weapon_less_fire":
        Upgrader.arch_get_attr_num(arch, "item_power", 3)
    elif artifact == "weapon_charisma":
        Upgrader.arch_get_attr_num(arch, "item_power", 3)
    elif artifact == "weapon_power":
        Upgrader.arch_get_attr_num(arch, "item_power", 3)
    elif artifact == "weapon_int":
        Upgrader.arch_get_attr_num(arch, "item_power", 3)
    elif artifact == "weapon_wisdom":
        Upgrader.arch_get_attr_num(arch, "item_power", 3)
    elif artifact == "weapon_const":
        Upgrader.arch_get_attr_num(arch, "item_power", 3)
    elif artifact == "weapon_dexterity":
        Upgrader.arch_get_attr_num(arch, "item_power", 3)
    elif artifact == "weapon_strength":
        Upgrader.arch_get_attr_num(arch, "item_power", 3)
    elif artifact == "weapon_assassin":
        Upgrader.arch_get_attr_num(arch, "item_power", 14)
    elif artifact == "weapon_slaying":
        Upgrader.arch_get_attr_num(arch, "item_power", 8)
    elif artifact == "weapon_damage":
        Upgrader.arch_get_attr_num(arch, "item_power", 3)
    elif artifact == "weapon_precision":
        Upgrader.arch_get_attr_num(arch, "item_power", 4)
    elif artifact == "weapon_accuracy":
        Upgrader.arch_get_attr_num(arch, "item_power", 3)
    elif artifact == "weapon_defense":
        Upgrader.arch_get_attr_num(arch, "item_power", 2)
    elif artifact == "weapon_fools":
        Upgrader.arch_get_attr_num(arch, "item_power", 1)
    elif artifact == "leggings_less_acid":
        Upgrader.arch_get_attr_num(arch, "item_power", 3)
    elif artifact == "leggings_less_poison":
        Upgrader.arch_get_attr_num(arch, "item_power", 3)
    elif artifact == "leggings_less_electricity":
        Upgrader.arch_get_attr_num(arch, "item_power", 3)
    elif artifact == "leggings_less_cold":
        Upgrader.arch_get_attr_num(arch, "item_power", 3)
    elif artifact == "leggings_less_fire":
        Upgrader.arch_get_attr_num(arch, "item_power", 3)
    elif artifact == "boots_less_acid":
        Upgrader.arch_get_attr_num(arch, "item_power", 3)
    elif artifact == "boots_less_poison":
        Upgrader.arch_get_attr_num(arch, "item_power", 3)
    elif artifact == "boots_less_elec":
        Upgrader.arch_get_attr_num(arch, "item_power", 3)
    elif artifact == "boots_less_cold":
        Upgrader.arch_get_attr_num(arch, "item_power", 3)
    elif artifact == "boots_less_fire":
        Upgrader.arch_get_attr_num(arch, "item_power", 3)
    elif artifact == "boots_granite":
        Upgrader.arch_get_attr_num(arch, "item_power", 3)
    elif artifact == "bracers_wis":
        Upgrader.arch_get_attr_num(arch, "item_power", 3)
    elif artifact == "bracers_pow":
        Upgrader.arch_get_attr_num(arch, "item_power", 3)
    elif artifact == "bracers_cha":
        Upgrader.arch_get_attr_num(arch, "item_power", 3)
    elif artifact == "bracers_int":
        Upgrader.arch_get_attr_num(arch, "item_power", 3)
    elif artifact == "bracers_con":
        Upgrader.arch_get_attr_num(arch, "item_power", 3)
    elif artifact == "bracers_str":
        Upgrader.arch_get_attr_num(arch, "item_power", 3)
    elif artifact == "bracers_dex":
        Upgrader.arch_get_attr_num(arch, "item_power", 3)
    elif artifact == "crown_stupidity":
        Upgrader.arch_get_attr_num(arch, "item_power", 1)
    elif artifact == "helm_argoth":
        Upgrader.arch_get_attr_num(arch, "item_power", 6)
    elif artifact == "crown_lordliness":
        Upgrader.arch_get_attr_num(arch, "item_power", 4)
    elif artifact == "crown_magi":
        Upgrader.arch_get_attr_num(arch, "item_power", 10)
    elif artifact == "crown_xebinon":
        Upgrader.arch_get_attr_num(arch, "item_power", 5)
    elif artifact == "helm_less_acid":
        Upgrader.arch_get_attr_num(arch, "item_power", 3)
    elif artifact == "helm_less_poison":
        Upgrader.arch_get_attr_num(arch, "item_power", 3)
    elif artifact == "helm_less_elec":
        Upgrader.arch_get_attr_num(arch, "item_power", 3)
    elif artifact == "helm_less_cold":
        Upgrader.arch_get_attr_num(arch, "item_power", 3)
    elif artifact == "helm_less_fire":
        Upgrader.arch_get_attr_num(arch, "item_power", 3)
    elif artifact == "helm_min_con":
        Upgrader.arch_get_attr_num(arch, "item_power", 3)
    elif artifact == "helm_min_cha":
        Upgrader.arch_get_attr_num(arch, "item_power", 3)
    elif artifact == "helm_min_pow":
        Upgrader.arch_get_attr_num(arch, "item_power", 3)
    elif artifact == "helm_min_wis":
        Upgrader.arch_get_attr_num(arch, "item_power", 3)
    elif artifact == "helm_min_int":
        Upgrader.arch_get_attr_num(arch, "item_power", 3)
    elif artifact == "helm_min_dex":
        Upgrader.arch_get_attr_num(arch, "item_power", 3)
    elif artifact == "helm_min_str":
        Upgrader.arch_get_attr_num(arch, "item_power", 3)
    elif artifact == "helm_xray":
        Upgrader.arch_get_attr_num(arch, "item_power", 7)
    elif artifact == "helm_infravision":
        Upgrader.arch_get_attr_num(arch, "item_power", 5)
    elif artifact == "gauntlet_precision":
        Upgrader.arch_get_attr_num(arch, "item_power", 5)
    elif artifact == "gauntlet_min_prec":
        Upgrader.arch_get_attr_num(arch, "item_power", 4)
    elif artifact == "gauntlet_min_dam":
        Upgrader.arch_get_attr_num(arch, "item_power", 4)
    elif artifact == "gauntlet_cha":
        Upgrader.arch_get_attr_num(arch, "item_power", 3)
    elif artifact == "gauntlet_wis":
        Upgrader.arch_get_attr_num(arch, "item_power", 3)
    elif artifact == "gauntlet_pow":
        Upgrader.arch_get_attr_num(arch, "item_power", 3)
    elif artifact == "gauntlet_min_int":
        Upgrader.arch_get_attr_num(arch, "item_power", 3)
    elif artifact == "gauntlet_min_con":
        Upgrader.arch_get_attr_num(arch, "item_power", 3)
    elif artifact == "gauntlet_min_dex":
        Upgrader.arch_get_attr_num(arch, "item_power", 3)
    elif artifact == "gauntlet_min_str":
        Upgrader.arch_get_attr_num(arch, "item_power", 3)
    elif artifact == "girdle_min_cha":
        Upgrader.arch_get_attr_num(arch, "item_power", 3)
    elif artifact == "girdle_min_wis":
        Upgrader.arch_get_attr_num(arch, "item_power", 3)
    elif artifact == "girdle_min_pow":
        Upgrader.arch_get_attr_num(arch, "item_power", 3)
    elif artifact == "girdle_min_int":
        Upgrader.arch_get_attr_num(arch, "item_power", 3)
    elif artifact == "girdle_min_con":
        Upgrader.arch_get_attr_num(arch, "item_power", 3)
    elif artifact == "girdle_min_dex":
        Upgrader.arch_get_attr_num(arch, "item_power", 3)
    elif artifact == "girdle_min_str":
        Upgrader.arch_get_attr_num(arch, "item_power", 3)
    elif artifact == "girdle_great_hp":
        Upgrader.arch_get_attr_num(arch, "item_power", 4)
    elif artifact == "girdle_major_hp":
        Upgrader.arch_get_attr_num(arch, "item_power", 3)
    elif artifact == "girdle_hp":
        Upgrader.arch_get_attr_num(arch, "item_power", 3)
    elif artifact == "girdle_medium_hp":
        Upgrader.arch_get_attr_num(arch, "item_power", 2)
    elif artifact == "girdle_minor_hp":
        Upgrader.arch_get_attr_num(arch, "item_power", 1)
    elif artifact == "shield_less_acid":
        Upgrader.arch_get_attr_num(arch, "item_power", 3)
    elif artifact == "shield_less_poison":
        Upgrader.arch_get_attr_num(arch, "item_power", 3)
    elif artifact == "shield_less_elec":
        Upgrader.arch_get_attr_num(arch, "item_power", 3)
    elif artifact == "shield_less_cold":
        Upgrader.arch_get_attr_num(arch, "item_power", 3)
    elif artifact == "shield_less_fire":
        Upgrader.arch_get_attr_num(arch, "item_power", 3)
    elif artifact == "shield_doom":
        Upgrader.arch_get_attr_num(arch, "item_power", 1)
    elif artifact == "shield_mass":
        Upgrader.arch_get_attr_num(arch, "item_power", 1)
    elif artifact == "armour_less_acid":
        Upgrader.arch_get_attr_num(arch, "item_power", 3)
    elif artifact == "armour_less_poison":
        Upgrader.arch_get_attr_num(arch, "item_power", 3)
    elif artifact == "armour_less_elec":
        Upgrader.arch_get_attr_num(arch, "item_power", 3)
    elif artifact == "armour_less_cold":
        Upgrader.arch_get_attr_num(arch, "item_power", 3)
    elif artifact == "armour_less_fire":
        Upgrader.arch_get_attr_num(arch, "item_power", 3)
    elif artifact == "armour_great_hp":
        Upgrader.arch_get_attr_num(arch, "item_power", 4)
    elif artifact == "armour_major_hp":
        Upgrader.arch_get_attr_num(arch, "item_power", 3)
    elif artifact == "armour_hp":
        Upgrader.arch_get_attr_num(arch, "item_power", 3)
    elif artifact == "armour_medium_hp":
        Upgrader.arch_get_attr_num(arch, "item_power", 2)
    elif artifact == "armour_minor_hp":
        Upgrader.arch_get_attr_num(arch, "item_power", 1)
    elif artifact == "armour_clumsiness":
        Upgrader.arch_get_attr_num(arch, "item_power", 1)
    elif artifact == "armour_doom":
        Upgrader.arch_get_attr_num(arch, "item_power", 1)
    elif artifact == "armour_mass":
        Upgrader.arch_get_attr_num(arch, "item_power", 1)
    elif artifact == "cape_med_cold":
        Upgrader.arch_get_attr_num(arch, "item_power", 5)
    elif artifact == "cape_med_fire":
        Upgrader.arch_get_attr_num(arch, "item_power", 5)
    elif artifact == "cape_woe":
        Upgrader.arch_get_attr_num(arch, "item_power", 1)
    elif artifact == "robe_min_cha":
        Upgrader.arch_get_attr_num(arch, "item_power", 2)
    elif artifact == "robe_min_wis":
        Upgrader.arch_get_attr_num(arch, "item_power", 2)
    elif artifact == "robe_min_pow":
        Upgrader.arch_get_attr_num(arch, "item_power", 2)
    elif artifact == "robe_min_int":
        Upgrader.arch_get_attr_num(arch, "item_power", 2)
    elif artifact == "robe_min_con":
        Upgrader.arch_get_attr_num(arch, "item_power", 2)
    elif artifact == "robe_min_dex":
        Upgrader.arch_get_attr_num(arch, "item_power", 2)
    elif artifact == "robe_min_str":
        Upgrader.arch_get_attr_num(arch, "item_power", 2)
    elif artifact == "robe_woe":
        Upgrader.arch_get_attr_num(arch, "item_power", 1)
    elif artifact == "cloak_less_acid":
        Upgrader.arch_get_attr_num(arch, "item_power", 3)
    elif artifact == "cloak_less_poison":
        Upgrader.arch_get_attr_num(arch, "item_power", 3)
    elif artifact == "cloak_less_elec":
        Upgrader.arch_get_attr_num(arch, "item_power", 3)
    elif artifact == "cloak_less_cold":
        Upgrader.arch_get_attr_num(arch, "item_power", 3)
    elif artifact == "cloak_less_fire":
        Upgrader.arch_get_attr_num(arch, "item_power", 3)
    elif artifact == "cloak_woe1":
        Upgrader.arch_get_attr_num(arch, "item_power", 1)

    return arch

upgrader = Upgrader.ObjectUpgrader(files, upgrade_func)
upgrader.upgrade()
