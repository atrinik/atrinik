'''
Implements the various object checkers.
'''
import re

import system.constants
from system.constants import game
import os
from system import utils

class AbstractChecker:
    def __init__(self, config):
        self.config = config
        self.errors = []
        self.fix = False

    def set_map_checker(self, map_checker):
        self.map_checker = map_checker

    def setPath(self, path):
        self.path = path

    def setName(self, name):
        self.name = name

    def check(self, obj, set_name = True):
        self.errors = []

        if set_name:
            self.path = self.name = obj.name

    def _check(self, obj):
        for name in dir(self):
            if name.startswith("checker_") and hasattr(getattr(self, name), "__call__"):
                getattr(self, name)(obj)

    def addError(self, severity, description, explanation, is_map_file = False, game_obj = None, loc = None, fixed = False, arch = None):
        extra = []

        if game_obj:
            extra.append("Object: <b>{}</b>".format(game_obj.getAttribute("name")))

        if arch or game_obj:
            extra.append("Arch name: <b>{}</b>".format(game_obj.name if game_obj else arch.name))

        if loc:
            extra.append("<b>X</b>: {}".format(loc[0]))
            extra.append("<b>Y</b>: {}".format(loc[1]))

        if extra:
            description += "&nbsp;<br>".join([""] + extra)

        error = {
                 "file": {
                         "name": self.name,
                         "path": self.path,
                         "is_map": is_map_file
                         },
                 "severity": "fixed" if fixed else severity,
                 "description": description,
                 "explanation": explanation,
                 "loc": loc,
        }

        self.errors.append(error)

class CheckerObject(AbstractChecker):
    def check(self, obj):
        super().check(obj, False)
        self._check_obj(obj)

    def _check_obj(self, obj):
        self._check(obj)

        for tmp in obj.inv:
            self._check_obj(tmp)

    def addError(self, *args, obj, fixed = False):
        env = obj.getParentTop()
        super().addError(*args, loc = [env.getAttributeInt("x"), env.getAttributeInt("y")], is_map_file = True, game_obj = obj, fixed = fixed)

    def _checker_msg(self, obj, msg):
        errors = []
        has_hello = False

        test_msg = re.sub(r"\[(\/?[a-z_]+)([^\]]*)\]", r"\1\2", msg)

        if test_msg.find("[") != -1 or test_msg.find("]") != -1:
            errors.append("unescaped-markup")

        for line in msg.split("\n"):
            if line.startswith("@match "):
                line = line[7:]

                if line.find("^hello$") != -1 and line != "^hello$":
                    errors.append("invalid-hello")

                if line == "^hello$":
                    has_hello = True

                parts = line.split("|")

                for part in parts:
                    if part == "*":
                        continue

                    if part[:1] != "^" or part[-1:] != "$":
                        errors.append("suspicious-regex")
            else:
                if line.find("[a") != -1:
                    errors.append("link-in-msg")

                if re.search(r"\^[^\^]*\^", line) or re.search(r"\|[^\|]*\|", line) or re.search(r"\~[^\~]*\~", line):
                    errors.append("control-chars")

        if not has_hello:
            errors.append("missing-hello")

        return errors

    def checker_misc(self, obj):
        if obj.getAttributeInt("direction") != 0 and obj.getAttributeInt("is_turnable") != 1 and obj.getAttributeInt("is_animated") != 1 and obj.getAttributeInt("draw_direction") != 1:
            self.addError("warning", "Object has direction but it doesn't support directions.", "Direction of the object is set, but it doesn't make use of it, since none of the following attributes are set: is_turnable, is_animated, draw_direction.", obj = obj)

        if obj.getAttributeInt("random_movement") == 1:
            if not obj.getAttributeInt("item_race") or not obj.getAttributeInt("item_level"):
                self.addError("low", "NPC has random movement enabled but no max movement range X/Y.", "Without a maximum X/Y set on the NPC, it can wander off endlessly, all the way through the world, which is typically not desireable.", obj = obj)

        if obj.getAttributeInt("is_turnable") == 1 and obj.getAttributeInt("draw_direction") == 1:
            if obj.getAttributeInt("direction") in (5, 6, 4, 3, 2, 8):
                self.addError("low", "Object has wrong direction set.", "Object with draw_direction flag set must be facing either west or north.", obj = obj)

        if obj.getAttribute("material") and not (obj.getAttribute("material_real") or obj.getAttribute("item_quality")) and obj.getAttributeInt("no_pick") == 0:
            self.addError("low", "Archetype has material set but no material_real or item_quality.", "Material requires material_real or item_quality to be set in order to work properly.", obj = obj)

        if obj.getAttribute("carrying"):
            self.addError("warning", "Object has carrying attribute set.", "Typically, the carrying attribute is reserved by the system, and map files should not contain it.", obj = obj)

        if obj.getAttribute("animation") == "NONE":
            self.addError("warning", "Object with animation set to NONE.", "Changing an object's animation to nothing is generally not recommended - setting is_animated to 0 is usually preferable.", obj = obj)

        if not obj.arch:
            return

        if self.config.getboolean("Errors", "layer_changed") and not obj.isSameArchAttribute("layer"):
            self.addError("warning", "Object with a modified layer.", "Changing layer of objects is generally not recommended, unless you know what you're doing.", obj = obj)

        if not obj.isSameArchAttribute("face"):
            if obj.getAttributeInt("is_turnable") == 1 or obj.getAttributeInt("is_animated") == 1:
                self.addError("warning", "Object is animated and/or turnable, but has had face changed.", "Changing face of turnable/animated objects is a waste, as their face will be set depending on their animation state (in case of animated objects), or the direction they're facing (in case of turnable objects).", obj = obj)

            if obj.getAttributeInt("type") == game.types.light_source:
                self.addError("warning", "Light source has had its face changed.", "Light sources automatically change their face, and changing it is not recommended.", obj = obj)

    def checker_types(self, obj):
        t = obj.getAttributeInt("type")

        if not t:
            if not obj.head:
                self.addError("critical", "Object has no type attribute set.", "All objects should have a type set.", obj = obj)

            return

        if t in (game.types.floor, game.types.shop_floor) and not obj.getAttributeInt("is_floor"):
            self.addError("low", "Floor archetype doesn't have is_floor set.", "All floor archetypes should have the is_floor attribute flag set to 1.", obj = obj)

        if t == game.types.magic_mirror and not obj.getAttributeInt("sys_object"):
            self.addError("medium", "Magic mirror archetype doesn't have sys_object set.", "All magic mirror archetypes should have the sys_object attribute flag tset to 1.", obj = obj)

        # Signs.
        if t == game.types.sign:
            if obj.getAttributeInt("walk_on") == 1 or obj.getAttributeInt("fly_on") == 1:
                if obj.getAttributeInt("splitting") == 1 and not obj.getAttributeInt("direction"):
                    self.addError("warning", "Magic mouth has adjacent direction setting set but has no actual direction.", "In order for the adjacent direction setting to work, a facing direction must be configured. The magic mouth will then activate for only the direction it's facing, and the two directions which are immediately adjacent to it.", obj = obj)

        if t in (game.types.door, game.types.gate, game.types.wall):
            if obj.getAttributeInt("damned") == 1:
                self.addError("low", "Object has the damned flag set.", "This flag is not supported on this object and it's typically an error.", obj = obj)

            if obj.getAttributeInt("no_magic") == 1:
                self.addError("warning", "Object has the no_magic flag set.", "This flag is usually set on floor objects, and it is likely an error on this object.", obj = obj)

        if t in (game.types.spawn_point_mob, game.types.magic_ear, game.types.book, game.types.sign):
            msg = obj.getAttribute("msg")

            if msg:
                is_mob_dialogue = t == game.types.spawn_point_mob
                is_ear_dialogue = t == game.types.magic_ear
                is_dialogue = is_mob_dialogue or is_ear_dialogue

                msg_errors = self._checker_msg(obj, msg)

                if is_mob_dialogue:
                    if "missing-hello" in msg_errors:
                        self.addError("low", "Dialogue @match error.", "Object has a @match dialogue is missing <b>@match ^hello$</b>.", obj = obj)

                if is_dialogue:
                    if "invalid-hello" in msg_errors:
                        self.addError("low", "Dialogue @match error.", "Object has a @match dialogue has invalid <b>@match ^hello$</b>.", obj = obj)

                    if "suspicious-regex" in msg_errors:
                        self.addError("low", "Dialogue @match error.", "Object has a @match that doesn't use regex.", obj = obj)

                    if "link-in-msg" in msg_errors:
                        self.addError("low", "Dialogue @match error.", "Object has a @match which uses links of some sort - this is not recommended.", obj = obj)

                if "control-chars" in msg_errors and self.config.getboolean("Errors", "deprecated_control_chars"):
                    self.addError("low", "Object contains deprecated control characters.", "Control characters have been deprecated in favour of special markup implementation.", obj = obj)

                if "unescaped-markup" in msg_errors:
                    self.addError("low", "Object contains unescaped markup in message.", "The following characters: [ and ] need to be replaced with: &amp;lsqb; and &amp;rsqb; respectively.", obj = obj)

        if t in (game.types.shop_mat, game.types.teleporter):
            self.addError("high", "Object has shop mat or teleporter object type.", "Shop mats and teleporters were merged into the exit object type.", obj = obj)

        if t == game.types.creator and obj.getAttribute("other_arch"):
            self.addError("critical", "Creator object with other_arch attribute.", "The other_arch attribute for creators was removed in favour of inventory objects.", obj = obj)

        # The following only applies to objects on the map.
        if not obj.map:
            return

        # Spawn points.
        if t == game.types.spawn_point and not obj.inv:
            if self.fix:
                obj.delete()

            self.addError("medium", "Empty spawn point object.", "Spawn point objects should generally have a monster inside their inventory that they will spawn.", obj = obj, fixed = self.fix)

        # Players.
        if t == game.types.player:
            if self.fix:
                obj.delete()

            self.addError("critical", "Player object on map.", "Player type objects are reserved system objects, and putting them on map will cause undefined behavior.", obj = obj, fixed = self.fix)

        # Monsters.
        if t == game.types.monster:
            self.addError("medium", "Monster type object on map.", "Generally, monsters should never be put directly on the map. Instead, they should be put into a spawn point's inventory, and their type changed to spawn point monster.", obj = obj)

        # Random drop.
        if t == game.types.random_drop:
            # Cannot be outside of inventory.
            if not obj.env:
                self.addError("high", "Random drop is outside of inventory.", "Random drops do not activate unless they are inside of an inventory.", obj = obj)

        # Quest container.
        if t == game.types.quest_container:
            sub_type = obj.getAttributeInt("sub_type")

            # Cannot be outside of inventory.
            if not obj.env:
                self.addError("high", "Quest container is outside of inventory.", "Quest containers do not trigger unless they are inside of an inventory.", obj = obj)

            need_name = sub_type != game.quest_container_sub_types.item_drop

            if not need_name:
                for tmp in obj.inv:
                    if tmp.getAttributeInt("one_drop") == 1:
                        need_name = True
                        break

            if obj.getAttribute("name") == obj.name:
                if need_name:
                    self.addError("high", "Quest container has no quest name.", "Quest containers should always have a quest name set, unless they're item drop quest type or they have a one_drop item.", obj = obj)
            else:
                if not need_name:
                    self.addError("low", "Quest container has a name.", "Quest containers with item drop quest type and no one_drop items should not have a quest name.", obj = obj)

        # Event objects.
        if t in (game.types.event_object, game.types.map_event_object):
            if obj.getAttribute("name") == obj.name:
                self.addError("high", "Event object is missing plugin name.", "Event objects must have name set to the plugin they should trigger.", obj = obj)
            elif not obj.getAttribute("name") in game.plugins:
                self.addError("critical", "Event object has unknown plugin: <b>{}</b>".format(obj.getAttribute("name")), "The following are valid plugin names: {}".format(", ".join(["<b>" + s + "</b>" for s in game.plugins])), obj = obj)

            if obj.getAttribute("race"):
                if obj.getAttribute("race").startswith("..") and obj.getAttribute("race").find("/python") != -1:
                    self.addError("warning", "Event object is using a relative path to the global /python directory.", "In general, it is recommended to use an absolute path to refer to scripts in the /python directory, such as, /python/generic/guard.py.", obj = obj)

        # Beacons.
        if t == game.types.beacon:
            if obj.getAttribute("name") == obj.name:
                self.addError("critical", "Beacon with no name set.", "Every beacon must have a unique name set.", obj = obj)
            elif obj.getAttribute("name") in self.map_checker.global_objects[game.types.beacon]:
                self.addError("critical", "Beacon with non-unique name.", "Beacon with the name <b>{}</b> already exists.".format(obj.getAttribute("name")), obj = obj)
            else:
                self.map_checker.global_objects[game.types.beacon].append(obj.getAttribute("name"))

    def checker_inventory_obj(self, obj):
        '''Checks attributes of objects that are inside another object.'''

        if not obj.env:
            return

        if obj.env.getAttributeInt("type") == game.types.spawn_point and not obj.getAttributeInt("type") in (game.types.spawn_point_mob, game.types.beacon, game.types.event_object):
            self.addError("high", "Incompatible object inside of a spawn point.", "Spawn points objects can only contain monsters, beacons and event objects.", obj = obj)

        if obj.getAttributeInt("type") == game.types.spawn_point:
            self.addError("high", "Spawn point object inside inventory of another object.", "Spawn points cannot work from inside an inventory of another object.", obj = obj)

        if obj.getAttributeInt("type") == game.types.exit and obj.env.getAttributeInt("type") != game.types.creator:
            self.addError("high", "Exit object inside inventory of another object.", "Exit objects can only be inside creators, as they have undefined behavior when inside any other object.", obj = obj)

        for attr in ["x", "y"]:
            if obj.getAttribute(attr) != None:
                self.addError("medium", "Object has {} attribute set but is inside inventory of another object.".format(attr.upper()), "Objects inside inventory of another object are technically not on any position on the map, so they should not have x/y attributes set.", obj = obj)

    def checker_inventory(self, obj):
        objs = {
                game.types.ability: [],
                game.types.waypoint: [],
                game.types.event_object: [],
                }

        for tmp in obj.inv:
            if tmp.getAttributeInt("type") in objs:
                objs[tmp.getAttributeInt("type")].append(tmp)

        for event in objs[game.types.event_object]:
            num = 0

            for event2 in objs[game.types.event_object]:
                if event.getAttributeInt("sub_type") == event2.getAttributeInt("sub_type"):
                    num += 1

            if num > 1:
                self.addError("low", "Object has events with two or more events with the same event type.", "It is generally recommended to only use one event maximum per event type.", obj = obj)
                break

        # Waypoints movement.
        if obj.getAttributeInt("movement_type") == 176:
            if not objs[game.types.waypoint]:
                self.addError("medium", "NPC has waypoint movement enabled but no waypoints configured.", "In order for NPCs to use waypoint movement, they must have waypoint objects in their inventory, which are properly configured.", obj = obj)
            else:
                for wp in objs[game.types.waypoint]:
                    if wp.getAttribute("name") == wp.name:
                        self.addError("medium", "Waypoint has no name.", "Waypoints should always have unique names (unique for every NPC, eg, <b>wp1</b>, <b>wp2</b>, etc) to identify which waypoint to use, and which waypoint to use next.", obj = obj)

                    title = wp.getAttribute("title")

                    if title:
                        found = False

                        for wp_next in objs[game.types.waypoint]:
                            if wp_next.getAttribute("name") == title:
                                found = True
                                break

                        if not found:
                            self.addError("high", "NPC has waypoint with nonexistant next waypoint.", "Waypoint <b>{}</b> is configured to use waypoint <b>{}</b> as next waypoint, but it doesn't exist.".format(wp.getAttribute("name"), title), obj = obj)
        elif objs[game.types.waypoint]:
            self.addError("warning", "NPC has waypoint movement disabled, but has waypoint objects in inventory.", "In order for NPC to use waypoints, waypoint movement must be enabled in movement settings.", obj = obj)

        if not obj.map:
            return

        if obj.getAttributeInt("can_cast_spell") == 1:
            if not objs[game.types.ability]:
                self.addError("low", "NPC can cast spells but has no ability objects.", "In order for the NPC to cast spells, ability objects that define which spells it can cast must be added to its inventory.", obj = obj)

            if obj.getAttributeInt("maxsp") == 0:
                self.addError("medium", "NPC can cast spells but has zero maximum mana.", "NPCs without any maximum mana will not be able to cast spells.", obj = obj)

            if obj.getAttributeInt("dex") == 0:
                self.addError("medium", "NPC can cast spells but has unset ability usage.", "In order for NPCs to cast their spells, they must have ability usage configured - this controls how often they will cast their spells or use their other abilities.", obj = obj)
        elif objs[game.types.ability]:
            self.addError("warning", "NPC cannot cast spells but has ability objects.", "In order for the NPC to be able to cast spells, it must have the 'can_cast_spell' flag on.", obj = obj)

    def checker_attributes(self, obj):
        artifact = self.map_checker.artifacts.get(obj.name)
        fix = obj.map is not None

        for attr in obj.getAttributes():
            val = obj.getAttribute(attr)

            if not attr.islower():
                self.addError("medium", "Attribute is not all lowercase: <b>{}</b>.".format(attr), "Officially, only lowercase attribute names are supported. Even though mixed-case attribute names still work today, they may not work in the future or on different platforms.", obj = obj, fixed = fix)

                if fix:
                    attr_old = attr
                    attr = attr.lower()
                    obj.replaceAttribute(attr_old, attr, val)

            game_attr = game.attributes.attrs.get(attr)

            if game_attr:
                if game_attr == game.attributes.INTEGER:
                    try:
                        int(val)
                    except:
                        self.addError("critical", "Attribute <b>{}</b> is supposed to be an integer, but is: {}".format(attr, val), "Attributes that do not have the correct data type could have highly unwanted side effects.", obj = obj)
                elif game_attr == game.attributes.BOOLEAN:
                    if not val in ("1", "0"):
                        self.addError("critical", "Attribute <b>{}</b> is supposed to be a boolean, but is: {}".format(attr, val), "Attributes that do not have the correct data type could have highly unwanted side effects.", obj = obj)
                elif game_attr == game.attributes.FLOAT:
                    try:
                        float(val)
                    except:
                        self.addError("critical", "Attribute <b>{}</b> is supposed to be a float, but is: {}".format(attr, val), "Attributes that do not have the correct data type could have highly unwanted side effects.", obj = obj)
            else:
                if self.config.getboolean("Errors", "unknown_attribute"):
                    self.addError("warning", "Attribute <b>{}</b> is not recognized.".format(attr), "Unrecognized attributes are loaded as custom attributes and can have special meaning in some object types. However, they could also be typos.", obj = obj)

            if not obj.map:
                continue

            if artifact and not attr in ("x", "y", "identified", "unpaid", "no_pick", "level", "nrof", "value", "can_stack", "layer", "sub_layer", "z", "zoom", "zoom_x", "zoom_y", "alpha", "align"):
                self.addError("high", "Artifact with modified attribute: <b>{}</b>.".format(attr), "Directly modifying attributes of most artifacts is not recommended, as it can result in artifacts with different statistics, found in different regions of the world, for example.<br><br>It is recommended to create a new artifact, rather than modifying an existing one on the map.", obj = obj)

            if obj.isSameArchAttribute(attr):
                if self.fix:
                    obj.removeAttribute(attr)

                self.addError("low", "Attribute <b>{}</b> is same as arch default.".format(attr), "This is often due to a change in archetypes, when the default value changes to something that has been set the same on a map.", obj = obj, fixed = self.fix)

    def checker_sys_object(self, obj):
        if obj.getAttributeInt("sys_object") == 1 and obj.getAttributeInt("layer") != 0:
            if not obj.env or not obj.env.getAttributeInt("type") in (game.types.spawn_point_mob, game.types.monster):
                if self.fix:
                    obj.setAttribute("layer", "0")

                self.addError("low", "System object has a non-zero layer set.", "System objects should always have layer 0.", obj = obj, fixed = self.fix)

        if obj.getAttributeInt("sys_object") == 0 and obj.getAttributeInt("layer") == 0:
            if self.fix:
                obj.setAttribute("sys_object", "1")

            self.addError("low", "Layer 0 object doesn't have sys_object set.", "Layer 0 objects should always have sys_object set.", obj = obj, fixed = self.fix)

    def checker_monster(self, obj):
        if not obj.getAttributeInt("type") in (game.types.spawn_point_mob, game.types.monster):
            return

        race = obj.getAttribute("race")

        if not race:
            self.addError("medium", "Monster without race set.", "All monsters should belong to a race.", obj = obj)
        elif race == "undead" and obj.getAttributeInt("undead") != 1:
            self.addError("medium", "Monster is of race <b>{}</b>, but doesn't have the undead flag set.".format(race), "Monsters that have their race set as undead should also have the undead flag set.", obj = obj)

        level = obj.getAttributeInt("level")

        if level == None:
            self.addError("high", "Monster has unset level.", "All monsters should have a level set.", obj = obj)
        elif level < 1 or level > system.constants.game.max_level:
            self.addError("high", "Monster has invalid level: <b>{}</b>".format(level), "Valid levels are between 1 and {}.".format(system.constants.game.max_level), obj = obj)

        if not obj.map:
            return

        if obj.getAttributeInt("friendly") == 0 and level >= 10 and obj.map.getAttributeInt("difficulty") <= 1:
            self.addError("medium", "Monster is level {} but map's difficulty is {}.".format(level, obj.map.getAttributeInt("difficulty")), "Generally, maps should have their difficulty set to average level of monsters it contains.", obj = obj)

        if obj.getAttributeInt("friendly") == 1 and not obj.getAttribute("name") in ("guard", "knight"):
            if obj.isSameArchAttribute("name") and obj.map.getAttribute("region") != "creation":
                has_say_event = False
                has_generic_guard_script = False

                for tmp in obj.inv:
                    if tmp.getAttributeInt("type") == game.types.event_object and tmp.getAttributeInt("sub_type") == 6:
                        if tmp.getAttribute("race") == "/python/generic/guard.py":
                            has_generic_guard_script = True

                        has_say_event = True
                        break

                if not has_generic_guard_script:
                    if obj.getAttribute("msg") or has_say_event:
                        self.addError("warning", "NPC has a dialog, but no custom name.", "NPCs with dialogs should always have a custom name set.", obj = obj)
            elif obj.getAttribute("name").istitle() and not re.match(r"^([A-Z][a-z\']*)( [A-Z][a-z\']*)?( (XC|XL|L?X{0,3})(IX|IV|V?I{0,3}))?$", obj.getAttribute("name")):
                self.addError("low", "NPC has name in incorrect format.", "NPCs should have their name in format such as <b>Ronald<b>, <b>Ronald Greyhammer</b>, etc.", obj = obj)

        if not obj.env or obj.env.getAttributeInt("type") != game.types.spawn_point:
            self.addError("critical", "Monster is not inside a spawn point.", "All monsters should always be inside a spawn point.", obj = obj)

    def checker_anim(self, obj):
        t = obj.getAttributeInt("type")

        if not obj.getAttributeInt("is_used_up") and obj.getAttributeInt("anim_speed") and obj.getAttributeFloat("speed") and not t in (game.types.monster, game.types.player, game.types.god, game.types.exit, game.types.cone, game.types.bullet, game.types.rod, game.types.spawn_point_mob, game.types.lightning, game.types.light_source):
            self.addError("warning", "Object is animated and has speed but its object type does not require speed.", "Animated objects don't require speed attribute to be set in order to be animated. Objects with speed are processed each tick server-side, using up unnecessary resources, since animations are processed client-side. Removing the speed attribute is recommended.", obj = obj)

class CheckerArchetype(CheckerObject):
    def check(self, obj):
        self.fix = False
        AbstractChecker.check(self, obj)
        super().check(obj)

    def addError(self, *args, obj, fixed = False):
        AbstractChecker.addError(self, *args, arch = obj)

class CheckerMap(AbstractChecker):
    def check(self, obj):
        super().check(obj)
        self.setName(obj.getAttribute("name"))

        self.map_checker.checker_object.setPath(self.path)
        self.map_checker.checker_object.setName(self.name)

        self._check(obj)

    def addError(self, *args, loc = None, game_obj = None, fixed = False):
        super().addError(*args, is_map_file = True, loc = loc, game_obj = game_obj, fixed = fixed)

    def _checker_game_object(self, game_obj):
        errors = []

        self.map_checker.checker_object.check(game_obj)
        errors += self.map_checker.checker_object.errors

        return errors

    def checker_tiled_maps(self, obj):
        tiles = []
        dirname = os.path.dirname(self.path)
        base = os.path.basename(self.path)
        coords = utils.MapCoords(base)
        tiled_check = os.path.realpath(self.path).startswith(
            os.path.realpath(self.map_checker.getMapsPath()))

        for i in range(0, system.constants.game.num_tiled):
            attribute = "tile_path_{}".format(i + 1)
            val = obj.getAttribute(attribute)

            if val is not None:
                if base == val:
                    if self.fix:
                        obj.removeAttribute(attribute)
                        val = None

                    self.addError("critical", "Map is tiled into itself ({} tile)".format(system.constants.game.tiled_names[i]), "Map cannot be tiled into itself.", fixed = self.fix)

            if val is not None:
                for tile in tiles:
                    if tile == val:
                        if self.fix:
                            obj.removeAttribute(attribute)
                            val = None

                        self.addError("critical", "Map is tiled to <b>{0}</b> more than once.".format(tile), "A map cannot have duplicate tile paths.", fixed = self.fix)

            if tiled_check and val is not None:
                if not os.path.exists(os.path.join(dirname, val)):
                    self.addError("critical", "Map has {} tile pointing to a file that does not exist: <b>{}</b>".format(system.constants.game.tiled_names[i], val), "A map tile cannot point to an invalid file.", fixed = self.fix)

                    if self.fix:
                        obj.removeAttribute(attribute)
                        val = None

            tiled = coords.getTiledName(i)

            # TODO: Should base this on something other than the map's file name
            # beginning with "world_".
            if tiled_check and (val is None or val != tiled) and (
                i < system.constants.game.num_tiled_dir or
                (base.startswith("world_") and coords.getLevel() >= 0 and (
                    i == system.constants.game.tiled_up or
                    coords.getLevel() > 0))):
                if os.path.exists(os.path.join(dirname, tiled)):
                    self.addError("high", "Map has {} tile pointing to <b>{}</b>, but it should be tiled to <b>{}</b>".format(system.constants.game.tiled_names[i], val if val is not None else "nothing", tiled), "Tiled map files follow a naming convention which allows programs to automatically determine the appropriate coordinates of a map in a mapset.", fixed = self.fix)

                    if self.fix:
                        obj.setAttribute(attribute, tiled)
                        val = tiled

            if val is not None:
                tiles.append(val)

    def checker_difficulty(self, obj):
        difficulty = obj.getAttributeInt("difficulty")

        if difficulty == None:
            if self.fix:
                obj.setAttribute("difficulty", 1)

            self.addError("low", "Map is missing difficulty.", "This could indicate an old map. Difficulty should be set between 1 and {}.".format(system.constants.game.max_level), fixed = self.fix)
        elif difficulty < 1 or difficulty > system.constants.game.max_level:
            if self.fix:
                obj.setAttribute("difficulty", 1 if difficulty < 1 else system.constants.game.max_level)

            self.addError("low", "Map has invalid difficulty set (<b>{}</b>).".format(difficulty), "Difficulty should be set between 1 and {}.".format(system.constants.game.max_level), fixed = self.fix)

    def checker_bg_music(self, obj):
        bg_music = obj.getAttribute("bg_music")

        if bg_music != None:
            if not re.match("([a-zA-Z0-9_\-]+)\.(\w+)[ 0-9\-]?", bg_music):
                self.addError("high", "Background music attribute (<b>{}</b>) is not in a valid format.".format(bg_music), "Valid format of the background music attribute is for example: ocean.ogg")
        else:
            if not obj.isWorldMap() and self.config.getboolean("Errors", "map_no_music"):
                self.addError("medium", "Background music is missing.", "Typically, every map (except empty world maps) should have some sort of background music set.")

    def checker_dimensions(self, obj):
        if obj.getAttribute("width") == None:
            self.addError("critical", "Map is missing width.", "A map without width attribute set is most likely corrupt.")

        if obj.getAttribute("width") == None:
            self.addError("critical", "Map is missing width.", "A map without height attribute set is most likely corrupt.")

    def checker_region(self, obj):
        region = obj.getAttribute("region")

        if region != None:
            if self.map_checker.regions.get(region) == None:
                self.addError("high", "Region <b>{}</b> is not a valid region.".format(region), "Make sure the region name is spelled correctly, or add it to the regions.reg file.")

            if obj.isWorldMap():
                if self.fix:
                    obj.removeAttribute("region")

                self.addError("warning", "Empty world map has a region.", "Empty world maps typically shouldn't have a region set.", fixed = self.fix)
        else:
            if not obj.isWorldMap() and self.config.getboolean("Errors", "map_no_region"):
                self.addError("medium", "Region is missing.", "Typically, every map (except empty world maps) should have a region set.")

    def checker_tiles(self, obj):
        for x in range(obj.getAttributeInt("width")):
            for y in range(obj.getAttributeInt("height")):
                if not x in obj.tiles or not y in obj.tiles[x]:
                    continue

                # Our layers.
                layers = [[0] * system.constants.game.num_sub_layers for i in range(system.constants.game.max_layers + 1)]
                # Number of objects. Layer 0 objects are not counted, and
                # neither are hidden objects.
                obj_count = 0
                # Total number of objects, with layer 0 objects.
                obj_count_all = 0
                is_shop = False
                sys_below_floor = False
                have_sys = False
                sys_not_on_top = False

                # Go through the objects on this map space.
                for game_obj in obj.tiles[x][y]:
                    # Recursively check the object.
                    self.errors += self._checker_game_object(game_obj)

                    # Get our layer and sub-layer.
                    layer = game_obj.getAttributeInt("layer")
                    sub_layer = game_obj.getAttributeInt("sub_layer")
                    # Increase number of layers.
                    layers[layer][sub_layer] += 1

                    # Increase number of objects, if we're not on layer 0 and
                    # the object is not hidden.
                    if layer != 0 and not game_obj.getAttributeInt("hidden"):
                        obj_count += 1

                    # The total count of objects.
                    obj_count_all += 1

                    if game_obj.getAttributeInt("type") == game.types.shop_floor:
                        is_shop = True

                    if layer == 0:
                        have_sys = True
                    elif have_sys:
                        if layer == 1:
                            sys_below_floor = True
                        else:
                            sys_not_on_top = True

                # No layer 1 objects and there are other non-layer-0 objects? Missing floor.
                if sum(layers[1]) == 0 and obj_count > 0:
                    self.addError("medium", "Missing layer 1 object on tile with some objects.", "This error is likely due to a missing floor object on this tile.<br><br>Floor must be on every tile that contains other objects that have layer other than zero.", loc = [x, y])

                # Go through the layers (ignoring layer 0), and check if we have more than one
                # object of the same layer on this space.
                for i in range(1, system.constants.game.max_layers + 1):
                    for j in range(0, system.constants.game.num_sub_layers):
                        if layers[i][j] > 1:
                            self.addError("warning", "More than 1 object ({}) with layer {}, sub-layer {} on same tile.".format(layers[i][j], i, j), "It is not recommended to place more than one object with the same layer and sub-layer on a single tile, as only one of them will appear on map in the client, and which one will appear is undefined.", loc = [x, y])

                for layer in [2, 3, 4]:
                    if sum(layers[5]) and sum(layers[layer]) and self.config.getboolean("Errors", "decor_wall_l" + str(layer)):
                        self.addError("warning", "Layer 5 object on tile with layer {} object(s).".format(layer), "It is generally not recommended to have layer {} objects on the same tile as wall objects.".format(layer), loc = [x, y])

                if sys_below_floor:
                    self.addError("low", "System object is below floor.", "It is considered bad practise to put system objects below floor tiles, as people editing the map may not see them and will not understand how certain aspects of the map work.", loc = [x, y])

                if sys_not_on_top and self.config.getboolean("Errors", "sys_not_on_top"):
                    self.addError("low", "System object is not on top.", "It is considered bad practise to put system objects below non-system objects, as people editing the map may not see them and will not understand how certain aspects of the map work.", loc = [x, y])

                if is_shop:
                    for game_obj in obj.tiles[x][y]:
                        if game_obj.getAttributeInt("sys_object") == 1 or game_obj.getAttributeInt("no_pick") == 1:
                            continue

                        if game_obj.getAttributeInt("unpaid") == 0:
                            self.addError("high", "Object <b>{}</b> is on a shop tile but is not unpaid.".format(game_obj.name), "All pickable objects inside shop tiles should have the unpaid attribute set, as otherwise they may be picked up from the shop without paying for them.", loc = [x, y], game_obj = game_obj)

