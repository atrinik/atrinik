"""
Implements general use constants.
"""


class ErrorLevel:
    """
    Implements the error level class; used for defining an error level,
    and its settings, such as color, etc.
    """

    def __init__(self, name):
        self.name = name
        self.colors = {}

    def set_color(self, colorType, value):
        """Sets the error level's color."""
        self.colors[colorType] = value

    def get_color(self, colorType):
        """Acquire error level's color."""
        return self.colors[colorType]


class ErrorLevelCollection:
    """Collection of error levels."""

    def __init__(self):
        self.warning = ErrorLevel("warning")
        self.low = ErrorLevel("low")
        self.medium = ErrorLevel("medium")
        self.high = ErrorLevel("high")
        self.critical = ErrorLevel("critical")
        self.fixed = ErrorLevel("fixed")

        self.warning.set_color("qt", "#000000")
        self.low.set_color("qt", "#FF00FF")
        self.medium.set_color("qt", "#00FFFF")
        self.high.set_color("qt", "#0000FF")
        self.critical.set_color("qt", "#ff0000")
        self.fixed.set_color("qt", "#00ff00")

    def __getitem__(self, key):
        return self.__dict__[key] if isinstance(self.__dict__[key],
                                                ErrorLevel) else None


class URLs:
    """URLs to links."""

    report_bug = "http://bugzilla.atrinik.org/"


class Game:
    """Game related constants."""

    max_level = 115
    world_map_name = "World"
    plugins = ["Python", "Arena"]
    # Highest layer value any archetype can have.
    max_layers = 7
    # Number of sub-layers.
    num_sub_layers = 7
    num_tiled = 10
    num_tiled_dir = 8
    tiled_up = 8
    tiled_down = 9
    tiled_names = ["north", "east", "south", "west", "northeast", "southeast",
                   "southwest", "northwest", "up", "down"]
    tiled_coords = [
        (0, 1, 0), (1, 0, 0), (0, -1, 0), (-1, 0, 0),
        (1, 1, 0), (1, -1, 0), (-1, -1, 0), (-1, 1, 0),
        (0, 0, 1), (0, 0, -1)
    ]

    class ServerCommands:
        control = 0

        control_map = 1
        control_map_reset = 1

        control_player = 2
        control_player_teleport = 1

    class Types:
        spawn_point = 81
        scroll = 111
        potion = 5
        monster = 80
        spawn_point_mob = 83
        random_drop = 102
        quest_container = 120
        ability = 110
        waypoint = 119
        player = 1
        exit = 66
        teleporter = 41
        floor = 71
        shop_floor = 68
        event_object = 118
        beacon = 126
        sign = 98
        creator = 42
        map_event_object = 127
        wall = 77
        magic_mirror = 28
        door = 20
        gate = 91
        book = 8
        magic_ear = 29
        light_source = 74
        bow = 14
        god = 50
        shop_mat = 69
        cone = 88
        bullet = 2
        rod = 3
        lightning = 12

    class QuestContainerSubTypes:
        none = 0
        kill = 1
        item = 2
        item_drop = 3
        special = 4

    class Attributes:
        STRING = 1
        INTEGER = 2
        BOOLEAN = 3
        FLOAT = 4

        attrs = {
            "msg": STRING,
            "name": STRING,
            "custom_name": STRING,
            "race": STRING,
            "slaying": STRING,
            "artifact": STRING,
            "amask": STRING,
            "other_arch": STRING,
            "animation": STRING,
            "inv_animation": STRING,
            "face": STRING,
            "inv_face": STRING,
            "title": STRING,
            "randomitems": STRING,

            "quickslot": INTEGER,
            "object_int1": INTEGER,
            "object_int2": INTEGER,
            "object_int3": INTEGER,
            "last_heal": INTEGER,
            "last_sp": INTEGER,
            "last_grace": INTEGER,
            "last_grace_add": INTEGER,
            "last_eat": INTEGER,
            "str": INTEGER,
            "dex": INTEGER,
            "con": INTEGER,
            "wis": INTEGER,
            "int": INTEGER,
            "pow": INTEGER,
            "cha": INTEGER,
            "hp": INTEGER,
            "maxhp": INTEGER,
            "sp": INTEGER,
            "maxsp": INTEGER,
            "exp": INTEGER,
            "food": INTEGER,
            "dam": INTEGER,
            "dam_add": INTEGER,
            "wc": INTEGER,
            "wc_add": INTEGER,
            "wc_range": INTEGER,
            "wc_range_add": INTEGER,
            "ac": INTEGER,
            "ac_add": INTEGER,
            "x": INTEGER,
            "y": INTEGER,
            "z": INTEGER,
            "zoom": INTEGER,
            "zoom_x": INTEGER,
            "zoom_y": INTEGER,
            "align": INTEGER,
            "alpha": INTEGER,
            "rotate": INTEGER,
            "nrof": INTEGER,
            "level": INTEGER,
            "direction": INTEGER,
            "type": INTEGER,
            "material": INTEGER,
            "value": INTEGER,
            "weight": INTEGER,
            "carrying": INTEGER,
            "path_attuned": INTEGER,
            "path_repelled": INTEGER,
            "path_denied": INTEGER,
            "magic": INTEGER,
            "state": INTEGER,
            "layer": INTEGER,
            "sub_layer": INTEGER,
            "run_away": INTEGER,
            "anim_speed": INTEGER,
            "container": INTEGER,
            "behavior": INTEGER,
            "attack_impact": INTEGER,
            "attack_slash": INTEGER,
            "attack_cleave": INTEGER,
            "attack_pierce": INTEGER,
            "attack_weaponmagic": INTEGER,
            "attack_fire": INTEGER,
            "attack_cold": INTEGER,
            "attack_electricity": INTEGER,
            "attack_poison": INTEGER,
            "attack_acid": INTEGER,
            "attack_magic": INTEGER,
            "attack_mind": INTEGER,
            "attack_blind": INTEGER,
            "attack_paralyze": INTEGER,
            "attack_force": INTEGER,
            "attack_godpower": INTEGER,
            "attack_chaos": INTEGER,
            "attack_drain": INTEGER,
            "attack_slow": INTEGER,
            "attack_confusion": INTEGER,
            "attack_internal": INTEGER,
            "protect_impact": INTEGER,
            "protect_slash": INTEGER,
            "protect_cleave": INTEGER,
            "protect_pierce": INTEGER,
            "protect_weaponmagic": INTEGER,
            "protect_fire": INTEGER,
            "protect_cold": INTEGER,
            "protect_electricity": INTEGER,
            "protect_poison": INTEGER,
            "protect_acid": INTEGER,
            "protect_magic": INTEGER,
            "protect_mind": INTEGER,
            "protect_blind": INTEGER,
            "protect_paralyze": INTEGER,
            "protect_force": INTEGER,
            "protect_godpower": INTEGER,
            "protect_chaos": INTEGER,
            "protect_drain": INTEGER,
            "protect_slow": INTEGER,
            "protect_confusion": INTEGER,
            "protect_internal": INTEGER,
            "movement_type": INTEGER,
            "attack_move_type": INTEGER,
            "move_state": INTEGER,
            "connected": INTEGER,
            "glow_radius": INTEGER,
            "sub_type": INTEGER,
            "terrain_flag": INTEGER,
            "terrain_type": INTEGER,
            "item_quality": INTEGER,
            "item_condition": INTEGER,
            "item_race": INTEGER,
            "item_skill": INTEGER,
            "item_level": INTEGER,
            "item_level_art": INTEGER,
            "material_real": INTEGER,
            "mpart_id": INTEGER,
            "mpart_nr": INTEGER,
            "item_power": INTEGER,

            "speed": FLOAT,
            "speed_left": FLOAT,
            "weapon_speed": FLOAT,

            "slow_move": BOOLEAN,
            "door_closed": BOOLEAN,
            "cursed_perm": BOOLEAN,
            "damned_perm": BOOLEAN,
            "one_drop": BOOLEAN,
            "is_trapped": BOOLEAN,
            "quest_item": BOOLEAN,
            "player_only": BOOLEAN,
            "is_named": BOOLEAN,
            "is_player": BOOLEAN,
            "sys_object": BOOLEAN,
            "can_stack": BOOLEAN,
            "is_thrown": BOOLEAN,
            "auto_apply": BOOLEAN,
            "is_assassin": BOOLEAN,
            "is_spell": BOOLEAN,
            "is_missile": BOOLEAN,
            "draw_direction": BOOLEAN,
            "draw_double": BOOLEAN,
            "draw_double_always": BOOLEAN,
            "see_invisible": BOOLEAN,
            "make_invisible": BOOLEAN,
            "make_ethereal": BOOLEAN,
            "can_roll": BOOLEAN,
            "connect_reset": BOOLEAN,
            "is_turnable": BOOLEAN,
            "is_used_up": BOOLEAN,
            "is_invisible": BOOLEAN,
            "applied": BOOLEAN,
            "unpaid": BOOLEAN,
            "hidden": BOOLEAN,
            "no_pick": BOOLEAN,
            "no_pass": BOOLEAN,
            "no_teleport": BOOLEAN,
            "corpse": BOOLEAN,
            "corpse_forced": BOOLEAN,
            "walk_on": BOOLEAN,
            "walk_off": BOOLEAN,
            "fly_on": BOOLEAN,
            "fly_off": BOOLEAN,
            "is_animated": BOOLEAN,
            "flying": BOOLEAN,
            "monster": BOOLEAN,
            "no_attack": BOOLEAN,
            "invulnerable": BOOLEAN,
            "friendly": BOOLEAN,
            "identified": BOOLEAN,
            "reflecting": BOOLEAN,
            "changing": BOOLEAN,
            "splitting": BOOLEAN,
            "hitback": BOOLEAN,
            "startequip": BOOLEAN,
            "blocksview": BOOLEAN,
            "undead": BOOLEAN,
            "scared": BOOLEAN,
            "unaggressive": BOOLEAN,
            "reflect_missile": BOOLEAN,
            "reflect_spell": BOOLEAN,
            "can_reflect_missile": BOOLEAN,
            "can_reflect_spell": BOOLEAN,
            "no_magic": BOOLEAN,
            "no_fix_player": BOOLEAN,
            "pass_thru": BOOLEAN,
            "can_pass_thru": BOOLEAN,
            "no_drop": BOOLEAN,
            "use_fix_pos": BOOLEAN,
            "is_ethereal": BOOLEAN,
            "two_handed": BOOLEAN,
            "can_cast_spell": BOOLEAN,
            "can_use_bow": BOOLEAN,
            "can_use_armour": BOOLEAN,
            "can_use_weapon": BOOLEAN,
            "has_ready_bow": BOOLEAN,
            "xrays": BOOLEAN,
            "no_save": BOOLEAN,
            "is_floor": BOOLEAN,
            "is_male": BOOLEAN,
            "is_female": BOOLEAN,
            "is_evil": BOOLEAN,
            "is_good": BOOLEAN,
            "is_neutral": BOOLEAN,
            "lifesave": BOOLEAN,
            "sleep": BOOLEAN,
            "stand_still": BOOLEAN,
            "random_move": BOOLEAN,
            "only_attack": BOOLEAN,
            "berserk": BOOLEAN,
            "is_magical": BOOLEAN,
            "connect_no_push": BOOLEAN,
            "connect_no_release": BOOLEAN,
            "confused": BOOLEAN,
            "stealth": BOOLEAN,
            "cursed": BOOLEAN,
            "damned": BOOLEAN,
            "is_buildable": BOOLEAN,
            "no_pvp": BOOLEAN,
            "been_applied": BOOLEAN,
            "outdoor": BOOLEAN,
            "unique": BOOLEAN,
            "has_ready_weapon": BOOLEAN,
            "no_skill_ident": BOOLEAN,
            "is_blind": BOOLEAN,
            "can_see_in_dark": BOOLEAN,
            "is_cauldron": BOOLEAN,
            "is_dust": BOOLEAN,
            "one_hit": BOOLEAN,
            "is_indestructible": BOOLEAN,

            "spawn_time": STRING,
            "stock": INTEGER,
            "faction": STRING,
            "faction_rep": INTEGER,
            "faction_kill_penalty": INTEGER,
            "name_female": STRING,
            "notification_message": STRING,
            "notification_action": STRING,
            "notification_shortcut": STRING,
            "notification_delay": INTEGER,
            "match": STRING,
        }
