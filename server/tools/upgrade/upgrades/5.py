# Upgrade for adding item_power to artifacts.

def upgrade_func(arch):
    if arch["archname"] in ("salt_phil", "salt", "lychee", "chilli", "orange", "cherries", "rambutan", "banana", "apricot", "grapes2", "pineapple", "strawberry", "lemon", "grapes", "peach", "apple", "bread2", "cookie", "figpud2", "cake", "icecake", "figpud", "cake_chocolate", "waybread", "bread", "champagne", "teabag", "beer", "wine", "cup_coffee", "water2", "drink_generic", "redwine", "whitewine", "booze2", "cup_tea", "booze_winter", "booze_generic", "eggnog", "choko", "salad", "melon", "snozzcumber", "carrot", "tomato", "brain", "stinger_insect", "head_demon", "ectoplasm", "liver", "blood", "residue", "head_goblin", "mint", "honeycomb", "clover", "mushroom1", "mushroom2", "mushroom3", "egg", "stick_weasel", "food_fish", "chicken_leg", "steak", "steak2", "drumstick", "bacon", "food_generic", "steak_dragon", "cheese", "cheeseburger", "pizza", "olive_oil", "bag_popcorn", "bonbons", "chocolate", "pipeweed"):
        # Remove any item quality; will default to 100 in archetype files.
        i = Upgrader.arch_get_attr_num(arch, "item_quality", None)

        if i != -1:
            arch["attrs"].pop(i)

        # Same as above, but for item condition.
        i = Upgrader.arch_get_attr_num(arch, "item_condition", None)

        if i != -1:
            arch["attrs"].pop(i)

        # Material should always be inherited from arc file.
        i = Upgrader.arch_get_attr_num(arch, "material", None)

        if i != -1:
            arch["attrs"].pop(i)

        # material_real is no longer used.
        i = Upgrader.arch_get_attr_num(arch, "material_real", None)

        if i != -1:
            arch["attrs"].pop(i)

        # Just in case...
        i = Upgrader.arch_get_attr_num(arch, "speed_left", None)

        if i != -1:
            arch["attrs"].pop(i)

    return arch

upgrader = Upgrader.ObjectUpgrader(files, upgrade_func)
upgrader.upgrade()
