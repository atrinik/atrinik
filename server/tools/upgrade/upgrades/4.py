# Upgrade for adding item_power to artifacts.

def upgrade_func(arch):
    artifact = Upgrader.arch_get_attr_val(arch, "artifact")

    if not artifact:
        return arch

    if artifact == "balm_first_aid":
        i = Upgrader.arch_get_attr_num(arch, "item_power", None)

        if i != -1 and arch["attrs"][i][1] != 0:
            arch["attrs"][i][1] = 0
    elif artifact == "potion_cure_illness":
        i = Upgrader.arch_get_attr_num(arch, "item_power", None)

        if i != -1 and arch["attrs"][i][1] != 0:
            arch["attrs"][i][1] = 0
    elif artifact == "potion_cure_sickness":
        i = Upgrader.arch_get_attr_num(arch, "item_power", None)

        if i != -1 and arch["attrs"][i][1] != 0:
            arch["attrs"][i][1] = 0
    elif artifact == "potion_freezing":
        i = Upgrader.arch_get_attr_num(arch, "item_power", None)

        if i != -1 and arch["attrs"][i][1] != 0:
            arch["attrs"][i][1] = 0
    elif artifact == "potion_firestorm":
        i = Upgrader.arch_get_attr_num(arch, "item_power", None)

        if i != -1 and arch["attrs"][i][1] != 0:
            arch["attrs"][i][1] = 0
    elif artifact == "potion_improve":
        i = Upgrader.arch_get_attr_num(arch, "item_power", None)

        if i != -1 and arch["attrs"][i][1] != 0:
            arch["attrs"][i][1] = 0
    elif artifact == "potion_minor_res":
        i = Upgrader.arch_get_attr_num(arch, "item_power", None)

        if i != -1 and arch["attrs"][i][1] != 0:
            arch["attrs"][i][1] = 0
    elif artifact == "potion_evil_liquid":
        i = Upgrader.arch_get_attr_num(arch, "item_power", None)

        if i != -1 and arch["attrs"][i][1] != 0:
            arch["attrs"][i][1] = 0
    elif artifact == "potion_minor_con":
        i = Upgrader.arch_get_attr_num(arch, "item_power", None)

        if i != -1 and arch["attrs"][i][1] != 0:
            arch["attrs"][i][1] = 0
    elif artifact == "potion_minor_dex":
        i = Upgrader.arch_get_attr_num(arch, "item_power", None)

        if i != -1 and arch["attrs"][i][1] != 0:
            arch["attrs"][i][1] = 0
    elif artifact == "potion_minor_strength":
        i = Upgrader.arch_get_attr_num(arch, "item_power", None)

        if i != -1 and arch["attrs"][i][1] != 0:
            arch["attrs"][i][1] = 0
    elif artifact == "potion_minor_res_acid":
        i = Upgrader.arch_get_attr_num(arch, "item_power", None)

        if i != -1 and arch["attrs"][i][1] != 0:
            arch["attrs"][i][1] = 0
    elif artifact == "potion_minor_res_poison":
        i = Upgrader.arch_get_attr_num(arch, "item_power", None)

        if i != -1 and arch["attrs"][i][1] != 0:
            arch["attrs"][i][1] = 0
    elif artifact == "potion_minor_res_elec":
        i = Upgrader.arch_get_attr_num(arch, "item_power", None)

        if i != -1 and arch["attrs"][i][1] != 0:
            arch["attrs"][i][1] = 0
    elif artifact == "potion_minor_res_cold":
        i = Upgrader.arch_get_attr_num(arch, "item_power", None)

        if i != -1 and arch["attrs"][i][1] != 0:
            arch["attrs"][i][1] = 0
    elif artifact == "potion_minor_res_fire":
        i = Upgrader.arch_get_attr_num(arch, "item_power", None)

        if i != -1 and arch["attrs"][i][1] != 0:
            arch["attrs"][i][1] = 0
    elif artifact == "scroll_recharge":
        i = Upgrader.arch_get_attr_num(arch, "item_power", None)

        if i != -1 and arch["attrs"][i][1] != 0:
            arch["attrs"][i][1] = 0
    elif artifact == "scroll_cause_lwound":
        i = Upgrader.arch_get_attr_num(arch, "item_power", None)

        if i != -1 and arch["attrs"][i][1] != 0:
            arch["attrs"][i][1] = 0
    elif artifact == "scroll_rem_damn":
        i = Upgrader.arch_get_attr_num(arch, "item_power", None)

        if i != -1 and arch["attrs"][i][1] != 0:
            arch["attrs"][i][1] = 0
    elif artifact == "scroll_rem_curse":
        i = Upgrader.arch_get_attr_num(arch, "item_power", None)

        if i != -1 and arch["attrs"][i][1] != 0:
            arch["attrs"][i][1] = 0
    elif artifact == "scroll_identify":
        i = Upgrader.arch_get_attr_num(arch, "item_power", None)

        if i != -1 and arch["attrs"][i][1] != 0:
            arch["attrs"][i][1] = 0
    elif artifact == "scroll_str_self":
        i = Upgrader.arch_get_attr_num(arch, "item_power", None)

        if i != -1 and arch["attrs"][i][1] != 0:
            arch["attrs"][i][1] = 0
    elif artifact == "scroll_min_heal":
        i = Upgrader.arch_get_attr_num(arch, "item_power", None)

        if i != -1 and arch["attrs"][i][1] != 0:
            arch["attrs"][i][1] = 0
    elif artifact == "scroll_icestorm":
        i = Upgrader.arch_get_attr_num(arch, "item_power", None)

        if i != -1 and arch["attrs"][i][1] != 0:
            arch["attrs"][i][1] = 0
    elif artifact == "scroll_firestorm":
        i = Upgrader.arch_get_attr_num(arch, "item_power", None)

        if i != -1 and arch["attrs"][i][1] != 0:
            arch["attrs"][i][1] = 0
    elif artifact == "food_grace_greater":
        i = Upgrader.arch_get_attr_num(arch, "item_power", None)

        if i != -1 and arch["attrs"][i][1] != 0:
            arch["attrs"][i][1] = 0
    elif artifact == "food_grace":
        i = Upgrader.arch_get_attr_num(arch, "item_power", None)

        if i != -1 and arch["attrs"][i][1] != 0:
            arch["attrs"][i][1] = 0
    elif artifact == "food_grace_medium":
        i = Upgrader.arch_get_attr_num(arch, "item_power", None)

        if i != -1 and arch["attrs"][i][1] != 0:
            arch["attrs"][i][1] = 0
    elif artifact == "food_grace_lesser":
        i = Upgrader.arch_get_attr_num(arch, "item_power", None)

        if i != -1 and arch["attrs"][i][1] != 0:
            arch["attrs"][i][1] = 0
    elif artifact == "food_mana_greater":
        i = Upgrader.arch_get_attr_num(arch, "item_power", None)

        if i != -1 and arch["attrs"][i][1] != 0:
            arch["attrs"][i][1] = 0
    elif artifact == "food_mana":
        i = Upgrader.arch_get_attr_num(arch, "item_power", None)

        if i != -1 and arch["attrs"][i][1] != 0:
            arch["attrs"][i][1] = 0
    elif artifact == "food_mana_medium":
        i = Upgrader.arch_get_attr_num(arch, "item_power", None)

        if i != -1 and arch["attrs"][i][1] != 0:
            arch["attrs"][i][1] = 0
    elif artifact == "food_mana_lesser":
        i = Upgrader.arch_get_attr_num(arch, "item_power", None)

        if i != -1 and arch["attrs"][i][1] != 0:
            arch["attrs"][i][1] = 0
    elif artifact == "sling_accuracy":
        i = Upgrader.arch_get_attr_num(arch, "item_power", None)

        if i != -1 and arch["attrs"][i][1] != 0:
            arch["attrs"][i][1] = 2
    elif artifact == "crossbow_accuracy":
        i = Upgrader.arch_get_attr_num(arch, "item_power", None)

        if i != -1 and arch["attrs"][i][1] != 0:
            arch["attrs"][i][1] = 2
    elif artifact == "range_accuracy":
        i = Upgrader.arch_get_attr_num(arch, "item_power", None)

        if i != -1 and arch["attrs"][i][1] != 0:
            arch["attrs"][i][1] = 2
    elif artifact == "missile_assassin":
        i = Upgrader.arch_get_attr_num(arch, "item_power", None)

        if i != -1 and arch["attrs"][i][1] != 0:
            arch["attrs"][i][1] = 0
    elif artifact == "missile_slaying":
        i = Upgrader.arch_get_attr_num(arch, "item_power", None)

        if i != -1 and arch["attrs"][i][1] != 0:
            arch["attrs"][i][1] = 0
    elif artifact == "missile_accuracy":
        i = Upgrader.arch_get_attr_num(arch, "item_power", None)

        if i != -1 and arch["attrs"][i][1] != 0:
            arch["attrs"][i][1] = 0
    elif artifact == "missile_inaccuracy":
        i = Upgrader.arch_get_attr_num(arch, "item_power", None)

        if i != -1 and arch["attrs"][i][1] != 0:
            arch["attrs"][i][1] = 0
    elif artifact == "amulet_calling_death":
        i = Upgrader.arch_get_attr_num(arch, "item_power", None)

        if i != -1 and arch["attrs"][i][1] != 0:
            arch["attrs"][i][1] = 1
    elif artifact == "amulet_sorrow":
        i = Upgrader.arch_get_attr_num(arch, "item_power", None)

        if i != -1 and arch["attrs"][i][1] != 0:
            arch["attrs"][i][1] = 1
    elif artifact == "amulet_shielding":
        i = Upgrader.arch_get_attr_num(arch, "item_power", None)

        if i != -1 and arch["attrs"][i][1] != 0:
            arch["attrs"][i][1] = 4
    elif artifact == "amulet_mithril":
        i = Upgrader.arch_get_attr_num(arch, "item_power", None)

        if i != -1 and arch["attrs"][i][1] != 0:
            arch["attrs"][i][1] = 0
    elif artifact == "amulet_adamant":
        i = Upgrader.arch_get_attr_num(arch, "item_power", None)

        if i != -1 and arch["attrs"][i][1] != 0:
            arch["attrs"][i][1] = 0
    elif artifact == "amulet_platinum":
        i = Upgrader.arch_get_attr_num(arch, "item_power", None)

        if i != -1 and arch["attrs"][i][1] != 0:
            arch["attrs"][i][1] = 0
    elif artifact == "amulet_gold":
        i = Upgrader.arch_get_attr_num(arch, "item_power", None)

        if i != -1 and arch["attrs"][i][1] != 0:
            arch["attrs"][i][1] = 0
    elif artifact == "amulet_silver":
        i = Upgrader.arch_get_attr_num(arch, "item_power", None)

        if i != -1 and arch["attrs"][i][1] != 0:
            arch["attrs"][i][1] = 0
    elif artifact == "amulet_bronze":
        i = Upgrader.arch_get_attr_num(arch, "item_power", None)

        if i != -1 and arch["attrs"][i][1] != 0:
            arch["attrs"][i][1] = 0
    elif artifact == "amulet_copper":
        i = Upgrader.arch_get_attr_num(arch, "item_power", None)

        if i != -1 and arch["attrs"][i][1] != 0:
            arch["attrs"][i][1] = 0
    elif artifact == "amulet_brass":
        i = Upgrader.arch_get_attr_num(arch, "item_power", None)

        if i != -1 and arch["attrs"][i][1] != 0:
            arch["attrs"][i][1] = 0
    elif artifact == "horn_fools":
        i = Upgrader.arch_get_attr_num(arch, "item_power", None)

        if i != -1 and arch["attrs"][i][1] != 0:
            arch["attrs"][i][1] = 0
    elif artifact == "horn_normal":
        i = Upgrader.arch_get_attr_num(arch, "item_power", None)

        if i != -1 and arch["attrs"][i][1] != 0:
            arch["attrs"][i][1] = 0
    elif artifact == "ring_woe3":
        i = Upgrader.arch_get_attr_num(arch, "item_power", None)

        if i != -1 and arch["attrs"][i][1] != 0:
            arch["attrs"][i][1] = 1
    elif artifact == "ring_woe2":
        i = Upgrader.arch_get_attr_num(arch, "item_power", None)

        if i != -1 and arch["attrs"][i][1] != 0:
            arch["attrs"][i][1] = 1
    elif artifact == "ring_woe1":
        i = Upgrader.arch_get_attr_num(arch, "item_power", None)

        if i != -1 and arch["attrs"][i][1] != 0:
            arch["attrs"][i][1] = 1
    elif artifact == "ring_doom":
        i = Upgrader.arch_get_attr_num(arch, "item_power", None)

        if i != -1 and arch["attrs"][i][1] != 0:
            arch["attrs"][i][1] = 1
    elif artifact == "ring_woe":
        i = Upgrader.arch_get_attr_num(arch, "item_power", None)

        if i != -1 and arch["attrs"][i][1] != 0:
            arch["attrs"][i][1] = 1
    elif artifact == "ring_benevolence":
        i = Upgrader.arch_get_attr_num(arch, "item_power", None)

        if i != -1 and arch["attrs"][i][1] != 0:
            arch["attrs"][i][1] = 12
    elif artifact == "ring_prelate":
        i = Upgrader.arch_get_attr_num(arch, "item_power", None)

        if i != -1 and arch["attrs"][i][1] != 0:
            arch["attrs"][i][1] = 10
    elif artifact == "ring_paladin":
        i = Upgrader.arch_get_attr_num(arch, "item_power", None)

        if i != -1 and arch["attrs"][i][1] != 0:
            arch["attrs"][i][1] = 5
    elif artifact == "ring_healer":
        i = Upgrader.arch_get_attr_num(arch, "item_power", None)

        if i != -1 and arch["attrs"][i][1] != 0:
            arch["attrs"][i][1] = 10

        i = Upgrader.arch_get_attr_num(arch, "path_denied", None)

        if i != -1:
            arch["attrs"][i][1] = 0

        Upgrader.arch_get_attr_num(arch, "path_repelled", 131072)
        Upgrader.arch_get_attr_num(arch, "Cha", 1)
        Upgrader.arch_get_attr_num(arch, "grace", 1)
    elif artifact == "ring_high_magic":
        i = Upgrader.arch_get_attr_num(arch, "item_power", None)

        if i != -1 and arch["attrs"][i][1] != 0:
            arch["attrs"][i][1] = 12
    elif artifact == "ring_ancient_magic":
        i = Upgrader.arch_get_attr_num(arch, "item_power", None)

        if i != -1 and arch["attrs"][i][1] != 0:
            arch["attrs"][i][1] = 10
    elif artifact == "ring_magic":
        i = Upgrader.arch_get_attr_num(arch, "item_power", None)

        if i != -1 and arch["attrs"][i][1] != 0:
            arch["attrs"][i][1] = 5
    elif artifact == "ring_strife":
        i = Upgrader.arch_get_attr_num(arch, "item_power", None)

        if i != -1 and arch["attrs"][i][1] != 0:
            arch["attrs"][i][1] = 11
    elif artifact == "ring_combat":
        i = Upgrader.arch_get_attr_num(arch, "item_power", None)

        if i != -1 and arch["attrs"][i][1] != 0:
            arch["attrs"][i][1] = 10
    elif artifact == "ring_ghost":
        i = Upgrader.arch_get_attr_num(arch, "item_power", None)

        if i != -1 and arch["attrs"][i][1] != 0:
            arch["attrs"][i][1] = 5
    elif artifact == "ring_yordan":
        i = Upgrader.arch_get_attr_num(arch, "item_power", None)

        if i != -1 and arch["attrs"][i][1] != 0:
            arch["attrs"][i][1] = 12
    elif artifact == "ring_ice_great":
        i = Upgrader.arch_get_attr_num(arch, "item_power", None)

        if i != -1 and arch["attrs"][i][1] != 0:
            arch["attrs"][i][1] = 10
    elif artifact == "ring_fire_great":
        i = Upgrader.arch_get_attr_num(arch, "item_power", None)

        if i != -1 and arch["attrs"][i][1] != 0:
            arch["attrs"][i][1] = 10
    elif artifact == "ring_storm_great":
        i = Upgrader.arch_get_attr_num(arch, "item_power", None)

        if i != -1 and arch["attrs"][i][1] != 0:
            arch["attrs"][i][1] = 10

        i = Upgrader.arch_get_attr_num(arch, "face", None)

        if i != -1:
            arch["attrs"][i][1] = "ring_storm.101"
    elif artifact == "ring_storm":
        i = Upgrader.arch_get_attr_num(arch, "item_power", None)

        if i != -1 and arch["attrs"][i][1] != 0:
            arch["attrs"][i][1] = 5
    elif artifact == "ring_fire":
        i = Upgrader.arch_get_attr_num(arch, "item_power", None)

        if i != -1 and arch["attrs"][i][1] != 0:
            arch["attrs"][i][1] = 5
    elif artifact == "ring_ice":
        i = Upgrader.arch_get_attr_num(arch, "item_power", None)

        if i != -1 and arch["attrs"][i][1] != 0:
            arch["attrs"][i][1] = 5
    elif artifact == "ring_thieves":
        i = Upgrader.arch_get_attr_num(arch, "item_power", None)

        if i != -1 and arch["attrs"][i][1] != 0:
            arch["attrs"][i][1] = 10
    elif artifact == "ring_mithril":
        i = Upgrader.arch_get_attr_num(arch, "item_power", None)

        if i != -1 and arch["attrs"][i][1] != 0:
            arch["attrs"][i][1] = 0
    elif artifact == "ring_adamant":
        i = Upgrader.arch_get_attr_num(arch, "item_power", None)

        if i != -1 and arch["attrs"][i][1] != 0:
            arch["attrs"][i][1] = 0
    elif artifact == "ring_platinum":
        i = Upgrader.arch_get_attr_num(arch, "item_power", None)

        if i != -1 and arch["attrs"][i][1] != 0:
            arch["attrs"][i][1] = 0
    elif artifact == "ring_gold":
        i = Upgrader.arch_get_attr_num(arch, "item_power", None)

        if i != -1 and arch["attrs"][i][1] != 0:
            arch["attrs"][i][1] = 0
    elif artifact == "ring_silver":
        i = Upgrader.arch_get_attr_num(arch, "item_power", None)

        if i != -1 and arch["attrs"][i][1] != 0:
            arch["attrs"][i][1] = 0
    elif artifact == "ring_bronze":
        i = Upgrader.arch_get_attr_num(arch, "item_power", None)

        if i != -1 and arch["attrs"][i][1] != 0:
            arch["attrs"][i][1] = 0
    elif artifact == "ring_copper":
        i = Upgrader.arch_get_attr_num(arch, "item_power", None)

        if i != -1 and arch["attrs"][i][1] != 0:
            arch["attrs"][i][1] = 0
    elif artifact == "ring_brass":
        i = Upgrader.arch_get_attr_num(arch, "item_power", None)

        if i != -1 and arch["attrs"][i][1] != 0:
            arch["attrs"][i][1] = 0
    elif artifact == "shield_holy_light":
        i = Upgrader.arch_get_attr_num(arch, "item_power", None)

        if i != -1 and arch["attrs"][i][1] != 0:
            arch["attrs"][i][1] = 15
    elif artifact == "boots_kriabe":
        i = Upgrader.arch_get_attr_num(arch, "item_power", None)

        if i != -1 and arch["attrs"][i][1] != 0:
            arch["attrs"][i][1] = 5
    elif artifact == "light_sandals":
        i = Upgrader.arch_get_attr_num(arch, "item_power", None)

        if i != -1 and arch["attrs"][i][1] != 0:
            arch["attrs"][i][1] = 1
    elif artifact == "amulet_life_saving":
        i = Upgrader.arch_get_attr_num(arch, "item_power", None)

        if i != -1 and arch["attrs"][i][1] != 0:
            arch["attrs"][i][1] = 3
    elif artifact == "gloves_drula":
        i = Upgrader.arch_get_attr_num(arch, "item_power", None)

        if i != -1 and arch["attrs"][i][1] != 0:
            arch["attrs"][i][1] = 5
    elif artifact == "armour_skel_lord":
        i = Upgrader.arch_get_attr_num(arch, "item_power", None)

        if i != -1 and arch["attrs"][i][1] != 0:
            arch["attrs"][i][1] = 5
    elif artifact == "cloak_rhun":
        i = Upgrader.arch_get_attr_num(arch, "item_power", None)

        if i != -1 and arch["attrs"][i][1] != 0:
            arch["attrs"][i][1] = 3
    elif artifact == "age_force_half_elf":
        i = Upgrader.arch_get_attr_num(arch, "item_power", None)

        if i != -1 and arch["attrs"][i][1] != 0:
            arch["attrs"][i][1] = 0
    elif artifact == "age_force_human":
        i = Upgrader.arch_get_attr_num(arch, "item_power", None)

        if i != -1 and arch["attrs"][i][1] != 0:
            arch["attrs"][i][1] = 0
    elif artifact == "amulet_normal":
        i = Upgrader.arch_get_attr_num(arch, "item_power", None)

        if i != -1 and arch["attrs"][i][1] != 0:
            arch["attrs"][i][1] = 0
    elif artifact == "ring_normal":
        i = Upgrader.arch_get_attr_num(arch, "item_power", None)

        if i != -1 and arch["attrs"][i][1] != 0:
            arch["attrs"][i][1] = 0
    elif artifact == "weapon_less_elec":
        i = Upgrader.arch_get_attr_num(arch, "item_power", None)

        if i != -1 and arch["attrs"][i][1] != 0:
            arch["attrs"][i][1] = 1
    elif artifact == "weapon_less_cold":
        i = Upgrader.arch_get_attr_num(arch, "item_power", None)

        if i != -1 and arch["attrs"][i][1] != 0:
            arch["attrs"][i][1] = 1
    elif artifact == "weapon_less_fire":
        i = Upgrader.arch_get_attr_num(arch, "item_power", None)

        if i != -1 and arch["attrs"][i][1] != 0:
            arch["attrs"][i][1] = 1
    elif artifact == "weapon_charisma":
        i = Upgrader.arch_get_attr_num(arch, "item_power", None)

        if i != -1 and arch["attrs"][i][1] != 0:
            arch["attrs"][i][1] = 1
    elif artifact == "weapon_power":
        i = Upgrader.arch_get_attr_num(arch, "item_power", None)

        if i != -1 and arch["attrs"][i][1] != 0:
            arch["attrs"][i][1] = 1
    elif artifact == "weapon_int":
        i = Upgrader.arch_get_attr_num(arch, "item_power", None)

        if i != -1 and arch["attrs"][i][1] != 0:
            arch["attrs"][i][1] = 1
    elif artifact == "weapon_wisdom":
        i = Upgrader.arch_get_attr_num(arch, "item_power", None)

        if i != -1 and arch["attrs"][i][1] != 0:
            arch["attrs"][i][1] = 1
    elif artifact == "weapon_const":
        i = Upgrader.arch_get_attr_num(arch, "item_power", None)

        if i != -1 and arch["attrs"][i][1] != 0:
            arch["attrs"][i][1] = 1
    elif artifact == "weapon_dexterity":
        i = Upgrader.arch_get_attr_num(arch, "item_power", None)

        if i != -1 and arch["attrs"][i][1] != 0:
            arch["attrs"][i][1] = 1
    elif artifact == "weapon_strength":
        i = Upgrader.arch_get_attr_num(arch, "item_power", None)

        if i != -1 and arch["attrs"][i][1] != 0:
            arch["attrs"][i][1] = 1
    elif artifact == "weapon_assassin":
        i = Upgrader.arch_get_attr_num(arch, "item_power", None)

        if i != -1 and arch["attrs"][i][1] != 0:
            arch["attrs"][i][1] = 6
    elif artifact == "weapon_slaying":
        i = Upgrader.arch_get_attr_num(arch, "item_power", None)

        if i != -1 and arch["attrs"][i][1] != 0:
            arch["attrs"][i][1] = 4
    elif artifact == "weapon_damage":
        i = Upgrader.arch_get_attr_num(arch, "item_power", None)

        if i != -1 and arch["attrs"][i][1] != 0:
            arch["attrs"][i][1] = 2
    elif artifact == "weapon_precision":
        i = Upgrader.arch_get_attr_num(arch, "item_power", None)

        if i != -1 and arch["attrs"][i][1] != 0:
            arch["attrs"][i][1] = 3
    elif artifact == "weapon_accuracy":
        i = Upgrader.arch_get_attr_num(arch, "item_power", None)

        if i != -1 and arch["attrs"][i][1] != 0:
            arch["attrs"][i][1] = 2
    elif artifact == "weapon_defense":
        i = Upgrader.arch_get_attr_num(arch, "item_power", None)

        if i != -1 and arch["attrs"][i][1] != 0:
            arch["attrs"][i][1] = 2
    elif artifact == "weapon_fools":
        i = Upgrader.arch_get_attr_num(arch, "item_power", None)

        if i != -1 and arch["attrs"][i][1] != 0:
            arch["attrs"][i][1] = 1
    elif artifact == "leggings_less_acid":
        i = Upgrader.arch_get_attr_num(arch, "item_power", None)

        if i != -1 and arch["attrs"][i][1] != 0:
            arch["attrs"][i][1] = 1
    elif artifact == "leggings_less_poison":
        i = Upgrader.arch_get_attr_num(arch, "item_power", None)

        if i != -1 and arch["attrs"][i][1] != 0:
            arch["attrs"][i][1] = 1
    elif artifact == "leggings_less_electricity":
        i = Upgrader.arch_get_attr_num(arch, "item_power", None)

        if i != -1 and arch["attrs"][i][1] != 0:
            arch["attrs"][i][1] = 1
    elif artifact == "leggings_less_cold":
        i = Upgrader.arch_get_attr_num(arch, "item_power", None)

        if i != -1 and arch["attrs"][i][1] != 0:
            arch["attrs"][i][1] = 1
    elif artifact == "leggings_less_fire":
        i = Upgrader.arch_get_attr_num(arch, "item_power", None)

        if i != -1 and arch["attrs"][i][1] != 0:
            arch["attrs"][i][1] = 1
    elif artifact == "boots_less_acid":
        i = Upgrader.arch_get_attr_num(arch, "item_power", None)

        if i != -1 and arch["attrs"][i][1] != 0:
            arch["attrs"][i][1] = 1
    elif artifact == "boots_less_poison":
        i = Upgrader.arch_get_attr_num(arch, "item_power", None)

        if i != -1 and arch["attrs"][i][1] != 0:
            arch["attrs"][i][1] = 1
    elif artifact == "boots_less_elec":
        i = Upgrader.arch_get_attr_num(arch, "item_power", None)

        if i != -1 and arch["attrs"][i][1] != 0:
            arch["attrs"][i][1] = 1
    elif artifact == "boots_less_cold":
        i = Upgrader.arch_get_attr_num(arch, "item_power", None)

        if i != -1 and arch["attrs"][i][1] != 0:
            arch["attrs"][i][1] = 1
    elif artifact == "boots_less_fire":
        i = Upgrader.arch_get_attr_num(arch, "item_power", None)

        if i != -1 and arch["attrs"][i][1] != 0:
            arch["attrs"][i][1] = 1
    elif artifact == "boots_granite":
        i = Upgrader.arch_get_attr_num(arch, "item_power", None)

        if i != -1 and arch["attrs"][i][1] != 0:
            arch["attrs"][i][1] = 3
    elif artifact == "bracers_wis":
        i = Upgrader.arch_get_attr_num(arch, "item_power", None)

        if i != -1 and arch["attrs"][i][1] != 0:
            arch["attrs"][i][1] = 1
    elif artifact == "bracers_pow":
        i = Upgrader.arch_get_attr_num(arch, "item_power", None)

        if i != -1 and arch["attrs"][i][1] != 0:
            arch["attrs"][i][1] = 1
    elif artifact == "bracers_cha":
        i = Upgrader.arch_get_attr_num(arch, "item_power", None)

        if i != -1 and arch["attrs"][i][1] != 0:
            arch["attrs"][i][1] = 1
    elif artifact == "bracers_int":
        i = Upgrader.arch_get_attr_num(arch, "item_power", None)

        if i != -1 and arch["attrs"][i][1] != 0:
            arch["attrs"][i][1] = 1
    elif artifact == "bracers_con":
        i = Upgrader.arch_get_attr_num(arch, "item_power", None)

        if i != -1 and arch["attrs"][i][1] != 0:
            arch["attrs"][i][1] = 1
    elif artifact == "bracers_str":
        i = Upgrader.arch_get_attr_num(arch, "item_power", None)

        if i != -1 and arch["attrs"][i][1] != 0:
            arch["attrs"][i][1] = 1
    elif artifact == "bracers_dex":
        i = Upgrader.arch_get_attr_num(arch, "item_power", None)

        if i != -1 and arch["attrs"][i][1] != 0:
            arch["attrs"][i][1] = 1
    elif artifact == "crown_stupidity":
        i = Upgrader.arch_get_attr_num(arch, "item_power", None)

        if i != -1 and arch["attrs"][i][1] != 0:
            arch["attrs"][i][1] = 1
    elif artifact == "helm_argoth":
        i = Upgrader.arch_get_attr_num(arch, "item_power", None)

        if i != -1 and arch["attrs"][i][1] != 0:
            arch["attrs"][i][1] = 2
    elif artifact == "crown_lordliness":
        i = Upgrader.arch_get_attr_num(arch, "item_power", None)

        if i != -1 and arch["attrs"][i][1] != 0:
            arch["attrs"][i][1] = 1
    elif artifact == "crown_magi":
        i = Upgrader.arch_get_attr_num(arch, "item_power", None)

        if i != -1 and arch["attrs"][i][1] != 0:
            arch["attrs"][i][1] = 8
    elif artifact == "crown_xebinon":
        i = Upgrader.arch_get_attr_num(arch, "item_power", None)

        if i != -1 and arch["attrs"][i][1] != 0:
            arch["attrs"][i][1] = 3
    elif artifact == "helm_less_acid":
        i = Upgrader.arch_get_attr_num(arch, "item_power", None)

        if i != -1 and arch["attrs"][i][1] != 0:
            arch["attrs"][i][1] = 1
    elif artifact == "helm_less_poison":
        i = Upgrader.arch_get_attr_num(arch, "item_power", None)

        if i != -1 and arch["attrs"][i][1] != 0:
            arch["attrs"][i][1] = 1
    elif artifact == "helm_less_elec":
        i = Upgrader.arch_get_attr_num(arch, "item_power", None)

        if i != -1 and arch["attrs"][i][1] != 0:
            arch["attrs"][i][1] = 1
    elif artifact == "helm_less_cold":
        i = Upgrader.arch_get_attr_num(arch, "item_power", None)

        if i != -1 and arch["attrs"][i][1] != 0:
            arch["attrs"][i][1] = 1
    elif artifact == "helm_less_fire":
        i = Upgrader.arch_get_attr_num(arch, "item_power", None)

        if i != -1 and arch["attrs"][i][1] != 0:
            arch["attrs"][i][1] = 1
    elif artifact == "helm_min_con":
        i = Upgrader.arch_get_attr_num(arch, "item_power", None)

        if i != -1 and arch["attrs"][i][1] != 0:
            arch["attrs"][i][1] = 1
    elif artifact == "helm_min_cha":
        i = Upgrader.arch_get_attr_num(arch, "item_power", None)

        if i != -1 and arch["attrs"][i][1] != 0:
            arch["attrs"][i][1] = 1
    elif artifact == "helm_min_pow":
        i = Upgrader.arch_get_attr_num(arch, "item_power", None)

        if i != -1 and arch["attrs"][i][1] != 0:
            arch["attrs"][i][1] = 1
    elif artifact == "helm_min_wis":
        i = Upgrader.arch_get_attr_num(arch, "item_power", None)

        if i != -1 and arch["attrs"][i][1] != 0:
            arch["attrs"][i][1] = 1
    elif artifact == "helm_min_int":
        i = Upgrader.arch_get_attr_num(arch, "item_power", None)

        if i != -1 and arch["attrs"][i][1] != 0:
            arch["attrs"][i][1] = 1
    elif artifact == "helm_min_dex":
        i = Upgrader.arch_get_attr_num(arch, "item_power", None)

        if i != -1 and arch["attrs"][i][1] != 0:
            arch["attrs"][i][1] = 1
    elif artifact == "helm_min_str":
        i = Upgrader.arch_get_attr_num(arch, "item_power", None)

        if i != -1 and arch["attrs"][i][1] != 0:
            arch["attrs"][i][1] = 1
    elif artifact == "helm_xray":
        i = Upgrader.arch_get_attr_num(arch, "item_power", None)

        if i != -1 and arch["attrs"][i][1] != 0:
            arch["attrs"][i][1] = 5
    elif artifact == "helm_infravision":
        i = Upgrader.arch_get_attr_num(arch, "item_power", None)

        if i != -1 and arch["attrs"][i][1] != 0:
            arch["attrs"][i][1] = 5
    elif artifact == "gauntlet_precision":
        i = Upgrader.arch_get_attr_num(arch, "item_power", None)

        if i != -1 and arch["attrs"][i][1] != 0:
            arch["attrs"][i][1] = 3
    elif artifact == "gauntlet_min_prec":
        i = Upgrader.arch_get_attr_num(arch, "item_power", None)

        if i != -1 and arch["attrs"][i][1] != 0:
            arch["attrs"][i][1] = 2
    elif artifact == "gauntlet_min_dam":
        i = Upgrader.arch_get_attr_num(arch, "item_power", None)

        if i != -1 and arch["attrs"][i][1] != 0:
            arch["attrs"][i][1] = 3
    elif artifact == "gauntlet_cha":
        i = Upgrader.arch_get_attr_num(arch, "item_power", None)

        if i != -1 and arch["attrs"][i][1] != 0:
            arch["attrs"][i][1] = 1
    elif artifact == "gauntlet_wis":
        i = Upgrader.arch_get_attr_num(arch, "item_power", None)

        if i != -1 and arch["attrs"][i][1] != 0:
            arch["attrs"][i][1] = 1
    elif artifact == "gauntlet_pow":
        i = Upgrader.arch_get_attr_num(arch, "item_power", None)

        if i != -1 and arch["attrs"][i][1] != 0:
            arch["attrs"][i][1] = 1
    elif artifact == "gauntlet_min_int":
        i = Upgrader.arch_get_attr_num(arch, "item_power", None)

        if i != -1 and arch["attrs"][i][1] != 0:
            arch["attrs"][i][1] = 1
    elif artifact == "gauntlet_min_con":
        i = Upgrader.arch_get_attr_num(arch, "item_power", None)

        if i != -1 and arch["attrs"][i][1] != 0:
            arch["attrs"][i][1] = 1
    elif artifact == "gauntlet_min_dex":
        i = Upgrader.arch_get_attr_num(arch, "item_power", None)

        if i != -1 and arch["attrs"][i][1] != 0:
            arch["attrs"][i][1] = 1
    elif artifact == "gauntlet_min_str":
        i = Upgrader.arch_get_attr_num(arch, "item_power", None)

        if i != -1 and arch["attrs"][i][1] != 0:
            arch["attrs"][i][1] = 1
    elif artifact == "girdle_min_cha":
        i = Upgrader.arch_get_attr_num(arch, "item_power", None)

        if i != -1 and arch["attrs"][i][1] != 0:
            arch["attrs"][i][1] = 1
    elif artifact == "girdle_min_wis":
        i = Upgrader.arch_get_attr_num(arch, "item_power", None)

        if i != -1 and arch["attrs"][i][1] != 0:
            arch["attrs"][i][1] = 1
    elif artifact == "girdle_min_pow":
        i = Upgrader.arch_get_attr_num(arch, "item_power", None)

        if i != -1 and arch["attrs"][i][1] != 0:
            arch["attrs"][i][1] = 1
    elif artifact == "girdle_min_int":
        i = Upgrader.arch_get_attr_num(arch, "item_power", None)

        if i != -1 and arch["attrs"][i][1] != 0:
            arch["attrs"][i][1] = 1
    elif artifact == "girdle_min_con":
        i = Upgrader.arch_get_attr_num(arch, "item_power", None)

        if i != -1 and arch["attrs"][i][1] != 0:
            arch["attrs"][i][1] = 1
    elif artifact == "girdle_min_dex":
        i = Upgrader.arch_get_attr_num(arch, "item_power", None)

        if i != -1 and arch["attrs"][i][1] != 0:
            arch["attrs"][i][1] = 1
    elif artifact == "girdle_min_str":
        i = Upgrader.arch_get_attr_num(arch, "item_power", None)

        if i != -1 and arch["attrs"][i][1] != 0:
            arch["attrs"][i][1] = 1
    elif artifact == "girdle_great_hp":
        i = Upgrader.arch_get_attr_num(arch, "item_power", None)

        if i != -1 and arch["attrs"][i][1] != 0:
            arch["attrs"][i][1] = 1
    elif artifact == "girdle_major_hp":
        i = Upgrader.arch_get_attr_num(arch, "item_power", None)

        if i != -1 and arch["attrs"][i][1] != 0:
            arch["attrs"][i][1] = 1
    elif artifact == "girdle_hp":
        i = Upgrader.arch_get_attr_num(arch, "item_power", None)

        if i != -1 and arch["attrs"][i][1] != 0:
            arch["attrs"][i][1] = 1
    elif artifact == "girdle_medium_hp":
        i = Upgrader.arch_get_attr_num(arch, "item_power", None)

        if i != -1 and arch["attrs"][i][1] != 0:
            arch["attrs"][i][1] = 1
    elif artifact == "girdle_minor_hp":
        i = Upgrader.arch_get_attr_num(arch, "item_power", None)

        if i != -1 and arch["attrs"][i][1] != 0:
            arch["attrs"][i][1] = 1
    elif artifact == "shield_less_acid":
        i = Upgrader.arch_get_attr_num(arch, "item_power", None)

        if i != -1 and arch["attrs"][i][1] != 0:
            arch["attrs"][i][1] = 1
    elif artifact == "shield_less_poison":
        i = Upgrader.arch_get_attr_num(arch, "item_power", None)

        if i != -1 and arch["attrs"][i][1] != 0:
            arch["attrs"][i][1] = 1
    elif artifact == "shield_less_elec":
        i = Upgrader.arch_get_attr_num(arch, "item_power", None)

        if i != -1 and arch["attrs"][i][1] != 0:
            arch["attrs"][i][1] = 1
    elif artifact == "shield_less_cold":
        i = Upgrader.arch_get_attr_num(arch, "item_power", None)

        if i != -1 and arch["attrs"][i][1] != 0:
            arch["attrs"][i][1] = 1
    elif artifact == "shield_less_fire":
        i = Upgrader.arch_get_attr_num(arch, "item_power", None)

        if i != -1 and arch["attrs"][i][1] != 0:
            arch["attrs"][i][1] = 1
    elif artifact == "shield_doom":
        i = Upgrader.arch_get_attr_num(arch, "item_power", None)

        if i != -1 and arch["attrs"][i][1] != 0:
            arch["attrs"][i][1] = 1
    elif artifact == "shield_mass":
        i = Upgrader.arch_get_attr_num(arch, "item_power", None)

        if i != -1 and arch["attrs"][i][1] != 0:
            arch["attrs"][i][1] = 1
    elif artifact == "armour_less_acid":
        i = Upgrader.arch_get_attr_num(arch, "item_power", None)

        if i != -1 and arch["attrs"][i][1] != 0:
            arch["attrs"][i][1] = 1
    elif artifact == "armour_less_poison":
        i = Upgrader.arch_get_attr_num(arch, "item_power", None)

        if i != -1 and arch["attrs"][i][1] != 0:
            arch["attrs"][i][1] = 1
    elif artifact == "armour_less_elec":
        i = Upgrader.arch_get_attr_num(arch, "item_power", None)

        if i != -1 and arch["attrs"][i][1] != 0:
            arch["attrs"][i][1] = 1
    elif artifact == "armour_less_cold":
        i = Upgrader.arch_get_attr_num(arch, "item_power", None)

        if i != -1 and arch["attrs"][i][1] != 0:
            arch["attrs"][i][1] = 1
    elif artifact == "armour_less_fire":
        i = Upgrader.arch_get_attr_num(arch, "item_power", None)

        if i != -1 and arch["attrs"][i][1] != 0:
            arch["attrs"][i][1] = 1
    elif artifact == "armour_great_hp":
        i = Upgrader.arch_get_attr_num(arch, "item_power", None)

        if i != -1 and arch["attrs"][i][1] != 0:
            arch["attrs"][i][1] = 1
    elif artifact == "armour_major_hp":
        i = Upgrader.arch_get_attr_num(arch, "item_power", None)

        if i != -1 and arch["attrs"][i][1] != 0:
            arch["attrs"][i][1] = 1
    elif artifact == "armour_hp":
        i = Upgrader.arch_get_attr_num(arch, "item_power", None)

        if i != -1 and arch["attrs"][i][1] != 0:
            arch["attrs"][i][1] = 1
    elif artifact == "armour_medium_hp":
        i = Upgrader.arch_get_attr_num(arch, "item_power", None)

        if i != -1 and arch["attrs"][i][1] != 0:
            arch["attrs"][i][1] = 1
    elif artifact == "armour_minor_hp":
        i = Upgrader.arch_get_attr_num(arch, "item_power", None)

        if i != -1 and arch["attrs"][i][1] != 0:
            arch["attrs"][i][1] = 1
    elif artifact == "armour_clumsiness":
        i = Upgrader.arch_get_attr_num(arch, "item_power", None)

        if i != -1 and arch["attrs"][i][1] != 0:
            arch["attrs"][i][1] = 1
    elif artifact == "armour_doom":
        i = Upgrader.arch_get_attr_num(arch, "item_power", None)

        if i != -1 and arch["attrs"][i][1] != 0:
            arch["attrs"][i][1] = 1
    elif artifact == "armour_mass":
        i = Upgrader.arch_get_attr_num(arch, "item_power", None)

        if i != -1 and arch["attrs"][i][1] != 0:
            arch["attrs"][i][1] = 1
    elif artifact == "cape_med_cold":
        i = Upgrader.arch_get_attr_num(arch, "item_power", None)

        if i != -1 and arch["attrs"][i][1] != 0:
            arch["attrs"][i][1] = 2
    elif artifact == "cape_med_fire":
        i = Upgrader.arch_get_attr_num(arch, "item_power", None)

        if i != -1 and arch["attrs"][i][1] != 0:
            arch["attrs"][i][1] = 2
    elif artifact == "cape_woe":
        i = Upgrader.arch_get_attr_num(arch, "item_power", None)

        if i != -1 and arch["attrs"][i][1] != 0:
            arch["attrs"][i][1] = 1
    elif artifact == "robe_min_cha":
        i = Upgrader.arch_get_attr_num(arch, "item_power", None)

        if i != -1 and arch["attrs"][i][1] != 0:
            arch["attrs"][i][1] = 1
    elif artifact == "robe_min_wis":
        i = Upgrader.arch_get_attr_num(arch, "item_power", None)

        if i != -1 and arch["attrs"][i][1] != 0:
            arch["attrs"][i][1] = 1
    elif artifact == "robe_min_pow":
        i = Upgrader.arch_get_attr_num(arch, "item_power", None)

        if i != -1 and arch["attrs"][i][1] != 0:
            arch["attrs"][i][1] = 1
    elif artifact == "robe_min_int":
        i = Upgrader.arch_get_attr_num(arch, "item_power", None)

        if i != -1 and arch["attrs"][i][1] != 0:
            arch["attrs"][i][1] = 1
    elif artifact == "robe_min_con":
        i = Upgrader.arch_get_attr_num(arch, "item_power", None)

        if i != -1 and arch["attrs"][i][1] != 0:
            arch["attrs"][i][1] = 1
    elif artifact == "robe_min_dex":
        i = Upgrader.arch_get_attr_num(arch, "item_power", None)

        if i != -1 and arch["attrs"][i][1] != 0:
            arch["attrs"][i][1] = 1
    elif artifact == "robe_min_str":
        i = Upgrader.arch_get_attr_num(arch, "item_power", None)

        if i != -1 and arch["attrs"][i][1] != 0:
            arch["attrs"][i][1] = 1
    elif artifact == "robe_woe":
        i = Upgrader.arch_get_attr_num(arch, "item_power", None)

        if i != -1 and arch["attrs"][i][1] != 0:
            arch["attrs"][i][1] = 1
    elif artifact == "cloak_less_acid":
        i = Upgrader.arch_get_attr_num(arch, "item_power", None)

        if i != -1 and arch["attrs"][i][1] != 0:
            arch["attrs"][i][1] = 1
    elif artifact == "cloak_less_poison":
        i = Upgrader.arch_get_attr_num(arch, "item_power", None)

        if i != -1 and arch["attrs"][i][1] != 0:
            arch["attrs"][i][1] = 1
    elif artifact == "cloak_less_elec":
        i = Upgrader.arch_get_attr_num(arch, "item_power", None)

        if i != -1 and arch["attrs"][i][1] != 0:
            arch["attrs"][i][1] = 1
    elif artifact == "cloak_less_cold":
        i = Upgrader.arch_get_attr_num(arch, "item_power", None)

        if i != -1 and arch["attrs"][i][1] != 0:
            arch["attrs"][i][1] = 1
    elif artifact == "cloak_less_fire":
        i = Upgrader.arch_get_attr_num(arch, "item_power", None)

        if i != -1 and arch["attrs"][i][1] != 0:
            arch["attrs"][i][1] = 1
    elif artifact == "cloak_woe1":
        i = Upgrader.arch_get_attr_num(arch, "item_power", None)

        if i != -1 and arch["attrs"][i][1] != 0:
            arch["attrs"][i][1] = 1
    elif artifact == "holding_weight":
        i = Upgrader.arch_get_attr_num(arch, "item_power", None)

        if i != -1 and arch["attrs"][i][1] != 0:
            arch["attrs"][i][1] = 0
    elif artifact == "holding_greater":
        i = Upgrader.arch_get_attr_num(arch, "item_power", None)

        if i != -1 and arch["attrs"][i][1] != 0:
            arch["attrs"][i][1] = 0
    elif artifact == "holding_great":
        i = Upgrader.arch_get_attr_num(arch, "item_power", None)

        if i != -1 and arch["attrs"][i][1] != 0:
            arch["attrs"][i][1] = 0
    elif artifact == "holding_fine":
        i = Upgrader.arch_get_attr_num(arch, "item_power", None)

        if i != -1 and arch["attrs"][i][1] != 0:
            arch["attrs"][i][1] = 0
    elif artifact == "holding":
        i = Upgrader.arch_get_attr_num(arch, "item_power", None)

        if i != -1 and arch["attrs"][i][1] != 0:
            arch["attrs"][i][1] = 0
    elif artifact == "holding_med":
        i = Upgrader.arch_get_attr_num(arch, "item_power", None)

        if i != -1 and arch["attrs"][i][1] != 0:
            arch["attrs"][i][1] = 0
    elif artifact == "holding_small":
        i = Upgrader.arch_get_attr_num(arch, "item_power", None)

        if i != -1 and arch["attrs"][i][1] != 0:
            arch["attrs"][i][1] = 0
    elif artifact == "holding_min":
        i = Upgrader.arch_get_attr_num(arch, "item_power", None)

        if i != -1 and arch["attrs"][i][1] != 0:
            arch["attrs"][i][1] = 0
    elif artifact == "opal_flawless":
        i = Upgrader.arch_get_attr_num(arch, "item_power", None)

        if i != -1 and arch["attrs"][i][1] != 0:
            arch["attrs"][i][1] = 0
    elif artifact == "opal_except":
        i = Upgrader.arch_get_attr_num(arch, "item_power", None)

        if i != -1 and arch["attrs"][i][1] != 0:
            arch["attrs"][i][1] = 0
    elif artifact == "opal_great":
        i = Upgrader.arch_get_attr_num(arch, "item_power", None)

        if i != -1 and arch["attrs"][i][1] != 0:
            arch["attrs"][i][1] = 0
    elif artifact == "opal_small":
        i = Upgrader.arch_get_attr_num(arch, "item_power", None)

        if i != -1 and arch["attrs"][i][1] != 0:
            arch["attrs"][i][1] = 0
    elif artifact == "opal_poor":
        i = Upgrader.arch_get_attr_num(arch, "item_power", None)

        if i != -1 and arch["attrs"][i][1] != 0:
            arch["attrs"][i][1] = 0
    elif artifact == "emerald_flawless":
        i = Upgrader.arch_get_attr_num(arch, "item_power", None)

        if i != -1 and arch["attrs"][i][1] != 0:
            arch["attrs"][i][1] = 0
    elif artifact == "emerald_except":
        i = Upgrader.arch_get_attr_num(arch, "item_power", None)

        if i != -1 and arch["attrs"][i][1] != 0:
            arch["attrs"][i][1] = 0
    elif artifact == "emerald_great":
        i = Upgrader.arch_get_attr_num(arch, "item_power", None)

        if i != -1 and arch["attrs"][i][1] != 0:
            arch["attrs"][i][1] = 0
    elif artifact == "emerald_small":
        i = Upgrader.arch_get_attr_num(arch, "item_power", None)

        if i != -1 and arch["attrs"][i][1] != 0:
            arch["attrs"][i][1] = 0
    elif artifact == "emerald_poor":
        i = Upgrader.arch_get_attr_num(arch, "item_power", None)

        if i != -1 and arch["attrs"][i][1] != 0:
            arch["attrs"][i][1] = 0
    elif artifact == "sapphire_flawless":
        i = Upgrader.arch_get_attr_num(arch, "item_power", None)

        if i != -1 and arch["attrs"][i][1] != 0:
            arch["attrs"][i][1] = 0
    elif artifact == "sapphire_except":
        i = Upgrader.arch_get_attr_num(arch, "item_power", None)

        if i != -1 and arch["attrs"][i][1] != 0:
            arch["attrs"][i][1] = 0
    elif artifact == "sapphire_great":
        i = Upgrader.arch_get_attr_num(arch, "item_power", None)

        if i != -1 and arch["attrs"][i][1] != 0:
            arch["attrs"][i][1] = 0
    elif artifact == "sapphire_small":
        i = Upgrader.arch_get_attr_num(arch, "item_power", None)

        if i != -1 and arch["attrs"][i][1] != 0:
            arch["attrs"][i][1] = 0
    elif artifact == "sapphire_poor":
        i = Upgrader.arch_get_attr_num(arch, "item_power", None)

        if i != -1 and arch["attrs"][i][1] != 0:
            arch["attrs"][i][1] = 0
    elif artifact == "ruby_flawless":
        i = Upgrader.arch_get_attr_num(arch, "item_power", None)

        if i != -1 and arch["attrs"][i][1] != 0:
            arch["attrs"][i][1] = 0
    elif artifact == "ruby_except":
        i = Upgrader.arch_get_attr_num(arch, "item_power", None)

        if i != -1 and arch["attrs"][i][1] != 0:
            arch["attrs"][i][1] = 0
    elif artifact == "ruby_great":
        i = Upgrader.arch_get_attr_num(arch, "item_power", None)

        if i != -1 and arch["attrs"][i][1] != 0:
            arch["attrs"][i][1] = 0
    elif artifact == "ruby_small":
        i = Upgrader.arch_get_attr_num(arch, "item_power", None)

        if i != -1 and arch["attrs"][i][1] != 0:
            arch["attrs"][i][1] = 0
    elif artifact == "ruby_poor":
        i = Upgrader.arch_get_attr_num(arch, "item_power", None)

        if i != -1 and arch["attrs"][i][1] != 0:
            arch["attrs"][i][1] = 0
    elif artifact == "diamond_flawless":
        i = Upgrader.arch_get_attr_num(arch, "item_power", None)

        if i != -1 and arch["attrs"][i][1] != 0:
            arch["attrs"][i][1] = 0
    elif artifact == "diamond_except":
        i = Upgrader.arch_get_attr_num(arch, "item_power", None)

        if i != -1 and arch["attrs"][i][1] != 0:
            arch["attrs"][i][1] = 0
    elif artifact == "diamond_great":
        i = Upgrader.arch_get_attr_num(arch, "item_power", None)

        if i != -1 and arch["attrs"][i][1] != 0:
            arch["attrs"][i][1] = 0
    elif artifact == "diamond_small":
        i = Upgrader.arch_get_attr_num(arch, "item_power", None)

        if i != -1 and arch["attrs"][i][1] != 0:
            arch["attrs"][i][1] = 0
    elif artifact == "diamond_poor":
        i = Upgrader.arch_get_attr_num(arch, "item_power", None)

        if i != -1 and arch["attrs"][i][1] != 0:
            arch["attrs"][i][1] = 0
    elif artifact == "amethyst_medium":
        i = Upgrader.arch_get_attr_num(arch, "item_power", None)

        if i != -1 and arch["attrs"][i][1] != 0:
            arch["attrs"][i][1] = 0
    elif artifact == "amethyst_small":
        i = Upgrader.arch_get_attr_num(arch, "item_power", None)

        if i != -1 and arch["attrs"][i][1] != 0:
            arch["attrs"][i][1] = 0
    elif artifact == "aquamarine_medium":
        i = Upgrader.arch_get_attr_num(arch, "item_power", None)

        if i != -1 and arch["attrs"][i][1] != 0:
            arch["attrs"][i][1] = 0
    elif artifact == "aquamarine_small":
        i = Upgrader.arch_get_attr_num(arch, "item_power", None)

        if i != -1 and arch["attrs"][i][1] != 0:
            arch["attrs"][i][1] = 0
    elif artifact == "jade_medium":
        i = Upgrader.arch_get_attr_num(arch, "item_power", None)

        if i != -1 and arch["attrs"][i][1] != 0:
            arch["attrs"][i][1] = 0
    elif artifact == "jade_small":
        i = Upgrader.arch_get_attr_num(arch, "item_power", None)

        if i != -1 and arch["attrs"][i][1] != 0:
            arch["attrs"][i][1] = 0
    elif artifact == "jasper_medium":
        i = Upgrader.arch_get_attr_num(arch, "item_power", None)

        if i != -1 and arch["attrs"][i][1] != 0:
            arch["attrs"][i][1] = 0
    elif artifact == "jasper_small":
        i = Upgrader.arch_get_attr_num(arch, "item_power", None)

        if i != -1 and arch["attrs"][i][1] != 0:
            arch["attrs"][i][1] = 0
    elif artifact == "zircon_medium":
        i = Upgrader.arch_get_attr_num(arch, "item_power", None)

        if i != -1 and arch["attrs"][i][1] != 0:
            arch["attrs"][i][1] = 0
    elif artifact == "zircon_small":
        i = Upgrader.arch_get_attr_num(arch, "item_power", None)

        if i != -1 and arch["attrs"][i][1] != 0:
            arch["attrs"][i][1] = 0
    elif artifact == "nug_mit_big":
        i = Upgrader.arch_get_attr_num(arch, "item_power", None)

        if i != -1 and arch["attrs"][i][1] != 0:
            arch["attrs"][i][1] = 0
    elif artifact == "nug_mit_med":
        i = Upgrader.arch_get_attr_num(arch, "item_power", None)

        if i != -1 and arch["attrs"][i][1] != 0:
            arch["attrs"][i][1] = 0
    elif artifact == "nug_mit_small":
        i = Upgrader.arch_get_attr_num(arch, "item_power", None)

        if i != -1 and arch["attrs"][i][1] != 0:
            arch["attrs"][i][1] = 0
    elif artifact == "nug_platin_big":
        i = Upgrader.arch_get_attr_num(arch, "item_power", None)

        if i != -1 and arch["attrs"][i][1] != 0:
            arch["attrs"][i][1] = 0
    elif artifact == "nug_platin_med":
        i = Upgrader.arch_get_attr_num(arch, "item_power", None)

        if i != -1 and arch["attrs"][i][1] != 0:
            arch["attrs"][i][1] = 0
    elif artifact == "nug_platin_small":
        i = Upgrader.arch_get_attr_num(arch, "item_power", None)

        if i != -1 and arch["attrs"][i][1] != 0:
            arch["attrs"][i][1] = 0
    elif artifact == "nug_gold_big":
        i = Upgrader.arch_get_attr_num(arch, "item_power", None)

        if i != -1 and arch["attrs"][i][1] != 0:
            arch["attrs"][i][1] = 0
    elif artifact == "nug_gold_med":
        i = Upgrader.arch_get_attr_num(arch, "item_power", None)

        if i != -1 and arch["attrs"][i][1] != 0:
            arch["attrs"][i][1] = 0
    elif artifact == "nug_gold_small":
        i = Upgrader.arch_get_attr_num(arch, "item_power", None)

        if i != -1 and arch["attrs"][i][1] != 0:
            arch["attrs"][i][1] = 0
    elif artifact == "nug_silver_big":
        i = Upgrader.arch_get_attr_num(arch, "item_power", None)

        if i != -1 and arch["attrs"][i][1] != 0:
            arch["attrs"][i][1] = 0
    elif artifact == "nug_silver_med":
        i = Upgrader.arch_get_attr_num(arch, "item_power", None)

        if i != -1 and arch["attrs"][i][1] != 0:
            arch["attrs"][i][1] = 0
    elif artifact == "nug_silver_small":
        i = Upgrader.arch_get_attr_num(arch, "item_power", None)

        if i != -1 and arch["attrs"][i][1] != 0:
            arch["attrs"][i][1] = 0
    elif artifact == "nug_bronze_big":
        i = Upgrader.arch_get_attr_num(arch, "item_power", None)

        if i != -1 and arch["attrs"][i][1] != 0:
            arch["attrs"][i][1] = 0
    elif artifact == "nug_bronze_med":
        i = Upgrader.arch_get_attr_num(arch, "item_power", None)

        if i != -1 and arch["attrs"][i][1] != 0:
            arch["attrs"][i][1] = 0
    elif artifact == "nug_bronze_small":
        i = Upgrader.arch_get_attr_num(arch, "item_power", None)

        if i != -1 and arch["attrs"][i][1] != 0:
            arch["attrs"][i][1] = 0
    elif artifact == "nug_copper_big":
        i = Upgrader.arch_get_attr_num(arch, "item_power", None)

        if i != -1 and arch["attrs"][i][1] != 0:
            arch["attrs"][i][1] = 0
    elif artifact == "nug_copper_med":
        i = Upgrader.arch_get_attr_num(arch, "item_power", None)

        if i != -1 and arch["attrs"][i][1] != 0:
            arch["attrs"][i][1] = 0
    elif artifact == "nug_copper_small":
        i = Upgrader.arch_get_attr_num(arch, "item_power", None)

        if i != -1 and arch["attrs"][i][1] != 0:
            arch["attrs"][i][1] = 0
    elif artifact == "nug_tin_big":
        i = Upgrader.arch_get_attr_num(arch, "item_power", None)

        if i != -1 and arch["attrs"][i][1] != 0:
            arch["attrs"][i][1] = 0
    elif artifact == "nug_tin_med":
        i = Upgrader.arch_get_attr_num(arch, "item_power", None)

        if i != -1 and arch["attrs"][i][1] != 0:
            arch["attrs"][i][1] = 0
    elif artifact == "nug_tin_small":
        i = Upgrader.arch_get_attr_num(arch, "item_power", None)

        if i != -1 and arch["attrs"][i][1] != 0:
            arch["attrs"][i][1] = 0
    elif artifact == "pearl_flawless_gold":
        i = Upgrader.arch_get_attr_num(arch, "item_power", None)

        if i != -1 and arch["attrs"][i][1] != 0:
            arch["attrs"][i][1] = 0
    elif artifact == "pearl_beauty_gold":
        i = Upgrader.arch_get_attr_num(arch, "item_power", None)

        if i != -1 and arch["attrs"][i][1] != 0:
            arch["attrs"][i][1] = 0
    elif artifact == "pearl_very_great_gold":
        i = Upgrader.arch_get_attr_num(arch, "item_power", None)

        if i != -1 and arch["attrs"][i][1] != 0:
            arch["attrs"][i][1] = 0
    elif artifact == "pearl_great_gold":
        i = Upgrader.arch_get_attr_num(arch, "item_power", None)

        if i != -1 and arch["attrs"][i][1] != 0:
            arch["attrs"][i][1] = 0
    elif artifact == "pearl_medium_gold":
        i = Upgrader.arch_get_attr_num(arch, "item_power", None)

        if i != -1 and arch["attrs"][i][1] != 0:
            arch["attrs"][i][1] = 0
    elif artifact == "pearl_poor_gold":
        i = Upgrader.arch_get_attr_num(arch, "item_power", None)

        if i != -1 and arch["attrs"][i][1] != 0:
            arch["attrs"][i][1] = 0
    elif artifact == "pearl_flawless_black":
        i = Upgrader.arch_get_attr_num(arch, "item_power", None)

        if i != -1 and arch["attrs"][i][1] != 0:
            arch["attrs"][i][1] = 0
    elif artifact == "pearl_beauty_black":
        i = Upgrader.arch_get_attr_num(arch, "item_power", None)

        if i != -1 and arch["attrs"][i][1] != 0:
            arch["attrs"][i][1] = 0
    elif artifact == "pearl_very_great_black":
        i = Upgrader.arch_get_attr_num(arch, "item_power", None)

        if i != -1 and arch["attrs"][i][1] != 0:
            arch["attrs"][i][1] = 0
    elif artifact == "pearl_great_black":
        i = Upgrader.arch_get_attr_num(arch, "item_power", None)

        if i != -1 and arch["attrs"][i][1] != 0:
            arch["attrs"][i][1] = 0
    elif artifact == "pearl_medium_black":
        i = Upgrader.arch_get_attr_num(arch, "item_power", None)

        if i != -1 and arch["attrs"][i][1] != 0:
            arch["attrs"][i][1] = 0
    elif artifact == "pearl_poor_black":
        i = Upgrader.arch_get_attr_num(arch, "item_power", None)

        if i != -1 and arch["attrs"][i][1] != 0:
            arch["attrs"][i][1] = 0
    elif artifact == "pearl_flawless":
        i = Upgrader.arch_get_attr_num(arch, "item_power", None)

        if i != -1 and arch["attrs"][i][1] != 0:
            arch["attrs"][i][1] = 0
    elif artifact == "pearl_beauty":
        i = Upgrader.arch_get_attr_num(arch, "item_power", None)

        if i != -1 and arch["attrs"][i][1] != 0:
            arch["attrs"][i][1] = 0
    elif artifact == "pearl_very_great":
        i = Upgrader.arch_get_attr_num(arch, "item_power", None)

        if i != -1 and arch["attrs"][i][1] != 0:
            arch["attrs"][i][1] = 0
    elif artifact == "pearl_great":
        i = Upgrader.arch_get_attr_num(arch, "item_power", None)

        if i != -1 and arch["attrs"][i][1] != 0:
            arch["attrs"][i][1] = 0
    elif artifact == "pearl_medium":
        i = Upgrader.arch_get_attr_num(arch, "item_power", None)

        if i != -1 and arch["attrs"][i][1] != 0:
            arch["attrs"][i][1] = 0
    elif artifact == "pearl_poor":
        i = Upgrader.arch_get_attr_num(arch, "item_power", None)

        if i != -1 and arch["attrs"][i][1] != 0:
            arch["attrs"][i][1] = 0
    elif artifact == "crystal_light4":
        i = Upgrader.arch_get_attr_num(arch, "item_power", None)

        if i != -1 and arch["attrs"][i][1] != 0:
            arch["attrs"][i][1] = 0

    return arch

upgrader = Upgrader.ObjectUpgrader(files, upgrade_func)
upgrader.upgrade()
