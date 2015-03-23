/*************************************************************************
 *           Atrinik, a Multiplayer Online Role Playing Game             *
 *                                                                       *
 *   Copyright (C) 2009-2014 Alex Tokar and Atrinik Development Team     *
 *                                                                       *
 * Fork from Crossfire (Multiplayer game for X-windows).                 *
 *                                                                       *
 * This program is free software; you can redistribute it and/or modify  *
 * it under the terms of the GNU General Public License as published by  *
 * the Free Software Foundation; either version 2 of the License, or     *
 * (at your option) any later version.                                   *
 *                                                                       *
 * This program is distributed in the hope that it will be useful,       *
 * but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 * GNU General Public License for more details.                          *
 *                                                                       *
 * You should have received a copy of the GNU General Public License     *
 * along with this program; if not, write to the Free Software           *
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.             *
 *                                                                       *
 * The author can be reached at admin@atrinik.org                        *
 ************************************************************************/

/**
 * @file
 * This file deals with things like flags, object types, and a lot
 * more.
 *
 * @todo
 * This file is really too large. With all the .h files
 * around, this file should be better split between them - things
 * that deal with objects should be in objects.h, things dealing
 * with players in player.h, etc. As it is, everything just seems
 * to be dumped in here. */

#ifndef DEFINE_H
#define DEFINE_H

#ifndef NAME_MAX
#define NAME_MAX 255
#endif

/** The maximum legal value of any stat. */
#define MAX_STAT            30
/** The minimum legal value of any stat. */
#define MIN_STAT            1

/**
 * @defgroup buffer_sizes Buffer sizes
 *@{*/
/** Used for all kinds of things. */
#define MAX_BUF             256
#define VERY_BIG_BUF        1024
/** Used for messages - some can be quite long. */
#define HUGE_BUF            4096
/** Maximum size of player name. */
#define MAX_NAME            16
#define BIG_NAME            32
#define MAX_EXT_TITLE       98
/*@}*/

#define MAX_ANIMATIONS      256

/**
 * @defgroup identify_modes Identify modes
 * Modes for cast_identify().
 *@{*/
/** Normal identification. */
#define IDENTIFY_NORMAL    0
/** Identify everything. */
#define IDENTIFY_ALL       1
/** Identify only marked item. */
#define IDENTIFY_MARKED    2
/*@}*/

/**
 * @defgroup spelllist_modes Spell list modes
 * Spell list commands for client spell list.
 *@{*/
/** Add a spell to the spell list */
#define SPLIST_MODE_ADD     0
/** Remove a spell from the spell list */
#define SPLIST_MODE_REMOVE  1
/** Update a spell in the spell list */
#define SPLIST_MODE_UPDATE  2
/*@}*/

/** Marks no skill used. */
#define CHOSEN_SKILL_NO (99999)

/** Town portal destination name */
#define PORTAL_DESTINATION_NAME "Town portal destination"
#define PORTAL_ACTIVE_NAME "Existing town portal"

/**
 * @defgroup los_blocked_modes LoS blocked modes
 * Line of Sight (los.c) defines
 *@{*/
/** It's visible. */
#define BLOCKED_LOS_VISIBLE     0x00
/** Ignore this tile for blocksview/visible changes. */
#define BLOCKED_LOS_IGNORE      0x01
/** Visible but will block all behind. */
#define BLOCKED_LOS_BLOCKSVIEW  0x02
/** Sight is blocked. */
#define BLOCKED_LOS_BLOCKED     0x04
/** Tile is not visible because it is not part of legal map. */
#define BLOCKED_LOS_OUT_OF_MAP  0x08
/*@}*/

/**
 * @defgroup type_defines Type defines
 * <h1>Type Defines</h1>
 * Only add new values to this list if somewhere in the program code,
 * it is actually needed. Just because you add a new monster does not
 * mean it has to have a type defined here. That only needs to happen
 * if in some .c file, it needs to do certain special actions based on
 * the monster type, that can not be handled by any of the numerous
 * flags.
 *
 * Also, if you add new entries, try and fill up the holes in this list.
 *
 * Type 0 will be undefined and show a non valid type information.
 *
 * Currently unused types to fill: 63, 67, 76, 97, 108, 128, 129,
 * 131, 132, 133, 134, 135, 136, 137, 140, 142, 143, 144, 145, 146, 147,
 * 148, 149, 150, 155, 151, 141, 107, 89, 61, 57, 45,
 * 46, 24.
 *@{*/
/**
 * The object is a player object. */
#define PLAYER 1
/**
 * A fired spell bullet. */
#define BULLET 2
/**
 * Rod. */
#define ROD 3
/**
 * An object that will generate treasure. */
#define TREASURE 4
/**
 * Potion. */
#define POTION 5
/**
 * Food. */
#define FOOD 6
/**
 * Book. */
#define BOOK 8
/**
 * Clock. Shows the in-game time. */
#define CLOCK 9
/**
 * Material used for the construction skill. */
#define MATERIAL 10
/**
 * Duplicator. */
#define DUPLICATOR 11
/**
 * Lightning. */
#define LIGHTNING 12
/**
 * An arrow. */
#define ARROW 13
/**
 * Bow. */
#define BOW 14
/**
 * Weapon. */
#define WEAPON 15
/**
 * Armour. */
#define ARMOUR 16
/**
 * Pedestal. */
#define PEDESTAL 17
/**
 * Confusion force. */
#define CONFUSION 19
/**
 * Door. */
#define DOOR 20
/**
 * Key to unlock @ref DOOR "a locked door". */
#define KEY 21
#define MAP 22
/**
 * A magic mirror which allows you to see objects on a different coordinate. */
#define MAGIC_MIRROR 28
/**
 * Spell information object. */
#define SPELL 29
/**
 * Shield. */
#define SHIELD 33
/**
 * Helmet. */
#define HELMET 34
/**
 * Greaves. */
#define GREAVES 35
/**
 * Money (copper, silver, etc). */
#define MONEY 36
/**
 * Object for applying character class modifications to someone. */
#define CLASS 37
/**
 * Objects that appear on place of player's death. */
#define GRAVESTONE 38
/**
 * An amulet. */
#define AMULET 39
/**
 * Player mover. */
#define PLAYER_MOVER 40
/**
 * Creator object. */
#define CREATOR 42
/**
 * Skill object. */
#define SKILL 43
/**
 * An experience object. */
#define EXPERIENCE 44
/**
 * Blindness force object. */
#define BLINDNESS 49
/**
 * God. */
#define GOD 50
/**
 * Detector is an object which notices the presence of another object and
 * is triggered like buttons. */
#define DETECTOR 51
/**
 * Item required to be equipped in order to use a skill. */
#define SKILL_ITEM 52
/**
 * Players become a DEAD_OBJECT when they logout. */
#define DEAD_OBJECT 53
/**
 * Drink. */
#define DRINK 54
/**
 * Inserts an invisible, weightless force into a player with a specified
 * string. */
#define MARKER 55
/**
 * Holy altar. */
#define HOLY_ALTAR 56
/**
 * Pearl. */
#define PEARL 59
/**
 * Gem. */
#define GEM 60
/**
 * Ambient sound effect. */
#define SOUND_AMBIENT 61
/**
 * Fire wall. */
#define FIREWALL 62
/**
 * Inventory checker. */
#define CHECK_INV 64
/**
 * Stairs, holes, portals, etc. */
#define EXIT 66
/**
 * Shop floor. */
#define SHOP_FLOOR 68
/**
 * A ring. */
#define RING 70
/**
 * This is a floor tile. */
#define FLOOR 71
/**
 * Animal body parts. */
#define FLESH 72
/**
 * Metals and minerals. */
#define INORGANIC 73
/**
 * Light source for players - torch, lantern, etc. */
#define LIGHT_APPLY 74
/**
 * This is a wall. */
#define WALL 77
/**
 * Light source on a map (invisible light). */
#define LIGHT_SOURCE 78
/**
 * Miscellaneous objects are for objects without a function in the
 * engine, like statues, clocks, chairs, etc. */
#define MISC_OBJECT 79
/**
 * A real living creature. */
#define MONSTER 80
/**
 * A spawn point object. */
#define SPAWN_POINT 81
/**
 * Refilling item for @ref LIGHT_APPLY. */
#define LIGHT_REFILL 82
/**
 * Monster inside a spawn point. */
#define SPAWN_POINT_MOB 83
/**
 * Used to find spawn point where monster came from. */
#define SPAWN_POINT_INFO 84
/**
 * Body parts which can't be eaten - dragon scales for example */
#define ORGANIC 86
/**
 * A cloak. */
#define CLOAK 87
/**
 * Cone spell. */
#define CONE 88
/**
 * Spinner. */
#define SPINNER 90
/**
 * Gate. */
#define GATE 91
/**
 * Button. */
#define BUTTON 92
/**
 * Handle. */
#define TYPE_HANDLE 93
/**
 * Special force for word of recall. */
#define WORD_OF_RECALL 96
/**
 * Sign. */
#define SIGN 98
/**
 * Boots. */
#define BOOTS 99
/**
 * Gloves. */
#define GLOVES 100
/**
 * This object holds the real base stats of monsters. */
#define BASE_INFO 101
/**
 * Only used for spawn point monsters:
 *
 * If found inside the monster, there is a random chance based on the
 * object's weight limit to drop the object's inventory into the
 * monster. */
#define RANDOM_DROP 102
/**
 * Bracers. */
#define BRACERS 104
/**
 * Poison force. */
#define POISONING 105
/**
 * A savebed. */
#define SAVEBED 106
/**
 * Wand. */
#define WAND 109
/**
 * Ability. */
#define ABILITY 110
/**
 * Scroll. */
#define SCROLL 111
/**
 * Director. */
#define DIRECTOR 112
/**
 * Girdle. */
#define GIRDLE 113
/**
 * A generic force object. */
#define FORCE 114
/**
 * A force, holding the effect of a potion. */
#define POTION_EFFECT 115
/**
 * A jewel. */
#define JEWEL 116
/**
 * Nugget. */
#define NUGGET 117
/**
 * Event/script object. */
#define EVENT_OBJECT 118
/**
 * Waypoint object. */
#define WAYPOINT_OBJECT 119
/**
 * Used to store quest information in players. */
#define QUEST_CONTAINER 120
/**
 * A container. */
#define CONTAINER 122
/**
 * This is a raw wealth object. When generated it's transformed into real
 * money depending on the environment where it's generated. */
#define WEALTH 125
/**
 * A beacon. */
#define BEACON 126
/**
 * Map event object. */
#define MAP_EVENT_OBJ 127
/**
 * Compass. */
#define COMPASS 151
/**
 * Map information object. */
#define MAP_INFO 152
/**
 * A swarm spell. */
#define SWARM_SPELL 153
/**
 * Rune. */
#define RUNE 154
/**
 * Client map information. */
#define CLIENT_MAP_INFO 155
/**
 * Power crystal. */
#define POWER_CRYSTAL 156
/**
 * Corpse. */
#define CORPSE 157
/**
 * Disease. */
#define DISEASE 158
/**
 * Disease symptom. */
#define SYMPTOM 159

#define OBJECT_TYPE_MAX 160
/*@}*/

/**
 * @defgroup sub_type_defines Sub type defines
 * The sub type defines.
 *@{*/

/**
 * @defgroup container_sub_types Container sub types
 * Containers can be different items: normal containers, mob corpse
 * or player corpse.
 *
 * All containers with sub_type higher or equal to 64 are personalized.
 * Personalized containers have name of the owner in their slaying
 * field and only the owner can open it.
 *
 * All containers with sub_type higher or equal to 128 are party
 * containers and have the party name stored in the slaying field. Only
 * the party members can open it.
 *@{*/
/** Normal container */
#define ST1_CONTAINER_NORMAL           0
/** Corpse container */
#define ST1_CONTAINER_CORPSE           1
/** Player container */
#define ST1_CONTAINER_DEAD_PL          2
/** Quiver. */
#define ST1_CONTAINER_QUIVER           3

/** Personalized normal container */
#define ST1_CONTAINER_NORMAL_player    64
/** Personalized corpse container */
#define ST1_CONTAINER_CORPSE_player    65

/** Party normal container */
#define ST1_CONTAINER_NORMAL_party     128
/** Party corpse container */
#define ST1_CONTAINER_CORPSE_party     129
/*@}*/

/**
 * @defgroup door_sub_types Door sub types
 * Sub types for DOOR objects.
 *@{*/
/** Normal door */
#define ST1_DOOR_NORMAL     0
/** Different sound */
#define ST1_DOOR_CURTAIN    1
/*@}*/

/**
 * @defgroup ST_BD_xxx Construction sub types
 * Sub types used for construction skill items.
 *@{*/
/** Destroys a previously built item. */
#define ST_BD_REMOVE 1
/** Builds marked material. */
#define ST_BD_BUILD 2
/*@}*/

/**
 * @defgroup ST_MAT_xxx Material sub types
 * Sub types used for construction materials.
 *@{*/
/** Floor material. */
#define ST_MAT_FLOOR 1
/** Wall material. */
#define ST_MAT_WALL 2
/** Object material. */
#define ST_MAT_ITEM 3
/** Window material. */
#define ST_MAT_WIN 4
/*@}*/

/**
 * @defgroup CLIENT_MAP_xxx Client map info types.
 * Sub types used for @ref CLIENT_MAP_INFO.
 *@{*/
/** Map label. */
#define CLIENT_MAP_LABEL 1
/** Tooltip. */
#define CLIENT_MAP_TOOLTIP 2
/** Hides part of a map. */
#define CLIENT_MAP_HIDE 3
/*@}*/

/*@}*/

/**
 * @defgroup size_of_free_defines Size of free defines
 * Instead of using arbitrary constants for indexing the
 * freearr, add these values.  <= SIZEOFFREE1 will get you
 * within 1 space.  <= SIZEOFFREE2 will get you within
 * 2 spaces, and the entire array (<= SIZEOFFREE) is
 * three spaces.
 *@{*/
#define SIZEOFFREE1     8
#define SIZEOFFREE2     24
#define SIZEOFFREE      49
/*@}*/

/** Number of different spells */
#define NROFREALSPELLS  52
/** Number of spell paths. */
#define NRSPELLPATHS    20

/**
 * @defgroup terrain_type_flags Terrain type flags
 * Terrain type flags
 * These values are used from terrain_type and terrain_flag
 * Arches without terrain flags become AIRBREATH as default.
 * These values also define the environment of the map tile position
 *@{*/
/** No terrain. */
#define TERRAIN_NOTHING         0
/** Walk on earth. */
#define TERRAIN_AIRBREATH       1
/** Walk on water - deep water too. */
#define TERRAIN_WATERWALK       2
/** You can move underwater and on land. */
#define TERRAIN_WATERBREATH     4
/** Walk over lava or fire, */
#define TERRAIN_FIREWALK        8
/** You can move in fire environment (fire elemental dimension, hell, ...) */
#define TERRAIN_FIREBREATH      16
/** Move "on clouds" in the air - not flying. */
#define TERRAIN_CLOUDWALK       32
/** Shallow water. */
#define TERRAIN_WATER_SHALLOW   64
/** Used in blocked() when we only want know about blocked by something. */
#define TERRAIN_ALL             0xffff
/*@}*/

/**
 * @defgroup flags_structure Flags Structure
 * Flag structure now changed.
 * Each flag is now a bit offset, starting at zero.  The macros
 * will update/read the appropriate flag element in the object
 * structure.
 *
 * Hopefully, since these offsets are integer constants set at run time,
 * the compiler will reduce the macros something as simple as the
 * old system was.
 *
 * Flags now have FLAG as the prefix.  This to be clearer, and also
 * to make sure F_ names are not still being used anyplace.
 *
 * The macros below assume that the flag size for each element is 32
 * bits.  IF it is smaller, bad things will happen.  See structs.h
 * for more info.
 *
 * All functions should use the macros below.  In process of converting
 * to the new system, I find several files that did not use the previous
 * macros.
 *
 * If any FLAG's are added, be sure to add them to the flag_links structure
 * in common/loader.c, if necessary.
 *
 * flags[0] is 0 to 31
 * flags[1] is 32 to 63
 * flags[2] is 64 to 95
 * flags[3] is 96 to 127
 *@{*/

/**
 * @defgroup object_flag_macros Object flag macros
 * Basic macros to do the above.
 *@{*/

/** Macro for getting flag's bitmask value. */
#define FLAG_BITMASK(p) (1U << (p % 32))

/**
 * Set flag of of an object.
 * @param xyz The object
 * @param p The flag to set */
#define SET_FLAG(xyz, p) \
    ((xyz)->flags[p / 32] |= FLAG_BITMASK(p))

/**
 * Clear flag of an object.
 * @param xyz The object
 * @param p The flag to clear */
#define CLEAR_FLAG(xyz, p) \
    ((xyz)->flags[p / 32] &= ~FLAG_BITMASK(p))

/**
 * Query flag of an object.
 * @param xyz The object
 * @param p The flag to query */
#define QUERY_FLAG(xyz, p) \
    ((xyz)->flags[p / 32] & FLAG_BITMASK(p))

/**
 * Toggle flag of an object.
 * @param xyz The object.
 * @param p The flag to toggle. */
#define TOGGLE_FLAG(xyz, p) \
    { \
        if (QUERY_FLAG((xyz), (p))) \
        { \
            CLEAR_FLAG((xyz), (p)); \
        } \
        else \
        { \
            SET_FLAG((xyz), (p)); \
        } \
    }
/*@}*/

/**
 * @defgroup object_multiflag_macros Object multiflag macros
 * This is rarely used but needed for some flags, which are
 * used for intern handling like INVISIBLE or WALK_OFF. Because
 * some core functions like object_remove() use this, it will be better
 * we set this ONE time outside instead of every time in object_remove():
 * we skip the call for the head in this way.
 *@{*/
#define SET_MULTI_FLAG(xyz, p)                          \
    {                                                       \
        object *_tos_;                                      \
                                                        \
        for (_tos_ = xyz; _tos_; _tos_ = _tos_->more)       \
        {                                                   \
            ((_tos_)->flags[p / 32] |= (1U << (p % 32)));   \
        }                                                   \
    }

#define CLEAR_MULTI_FLAG(xyz, p)                        \
    {                                                       \
        object *_tos_;                                      \
                                                        \
        for (_tos_ = xyz; _tos_; _tos_ = _tos_->more)       \
        {                                                   \
            ((_tos_)->flags[p / 32] &= ~(1U << (p % 32)));  \
        }                                                   \
    }

#define SET_OR_CLEAR_MULTI_FLAG(_head, _flag) \
    if (QUERY_FLAG((_head), (_flag))) \
    { \
        SET_MULTI_FLAG((_head)->more, (_flag)); \
    } \
    else \
    { \
        CLEAR_MULTI_FLAG((_head)->more, (_flag)); \
    }

#define SET_OR_CLEAR_MULTI_FLAG_IF_CLONE(_head, _flag) \
    if (QUERY_FLAG((_head), (_flag)) && !QUERY_FLAG(&(_head)->arch->clone, (_flag))) \
    { \
        SET_MULTI_FLAG((_head)->more, (_flag)); \
    } \
    else if (!QUERY_FLAG((_head), (_flag)) && QUERY_FLAG(&(_head)->arch->clone, (_flag))) \
    { \
        CLEAR_MULTI_FLAG((_head)->more, (_flag)); \
    }
/*@}*/

/**
 * @defgroup item_types Item types
 * Convenience macros to determine what kind of things we are dealing with.
 *@{*/
/** Is this a weapon? */
#define IS_WEAPON(op) (op->type == ARROW || op->type == BOW || op->type == WEAPON)
/** Is this some kind of armor (shield, helmet, cloak, etc)? */
#define IS_ARMOR(op) (op->type == ARMOUR || op->type == SHIELD || op->type == HELMET || op->type == CLOAK || op->type == BOOTS || op->type == GLOVES || op->type == BRACERS || op->type == GIRDLE || op->type == GREAVES)
/** Is this object alive? */
#define IS_LIVE(op) ((op)->type == PLAYER || QUERY_FLAG((op), FLAG_MONSTER))
/** Is it an arrow? */
#define IS_ARROW(op) (op->type == ARROW || op->type == BULLET)
/** Determine whether the object is an attack spell. */
#define IS_ATTACK_SPELL(op) ((op)->type == BULLET || (op)->type == CONE || (op)->type == LIGHTNING)
/*@}*/

/**
 * @defgroup flag_defines Flag defines
 * The object flags.
 *
 * Unused: 77, 78, 128, 98, 81, 82, 104, 105, 106, 107, 108,
 * 109, 96, 87, 88, 54, 53, 89, 79, 44, 48, 19.
 *
 * @note
 * ::object_flag_names has text-representations of these flags, used for
 * saving the flags and accessing them from Python plugin.
 *
 * @warning
 * The first 8 bit are used from the map2 cmd as direct mapped data.
 * The order must stay as it is here!
 * @{*/
/**
 * Monster is sleeping. While active, the monster's visibility range is
 * reduced. */
#define FLAG_SLEEP 0
/**
 * Confused. Random direction when moving. */
#define FLAG_CONFUSED 1
/**
 * Paralyzed, cannot do various movement-related actions. */
#define FLAG_PARALYZED 2
/**
 * Monster is scared. */
#define FLAG_SCARED 3
/**
 * Object cannot see the map with eyes, and cannot read books/scrolls/etc. */
#define FLAG_BLIND 4
/**
 * Can only be see by objects with @ref FLAG_SEE_INVISIBLE. */
#define FLAG_IS_INVISIBLE 5
/**
 * Object is ethereal. */
#define FLAG_IS_ETHEREAL 6
/**
 * Alignment flag. */
#define FLAG_IS_GOOD 7
/**
 * Object can't be picked up. */
#define FLAG_NO_PICK 8
/**
 * Applied when it's walked upon. */
#define FLAG_WALK_ON 9
/**
 * Nothing can pass. */
#define FLAG_NO_PASS 10
/**
 * The object is animated. */
#define FLAG_ANIMATE 11
/**
 * Uses the living::exp to slow down movement. */
#define FLAG_SLOW_MOVE 12
/** The object is flying. */
#define FLAG_FLYING 13
/**
 * The object is a monster, golem, etc. */
#define FLAG_MONSTER 14
/**
 * The monster is friendly and will not attack other friendly objects. */
#define FLAG_FRIENDLY 15
/**
 * Object is not in any map or inventory. */
#define FLAG_REMOVED 16
/**
 * The object has been applied before. */
#define FLAG_BEEN_APPLIED 17
/**
 * Will be applied when created (treasure chest for example). */
#define FLAG_AUTO_APPLY 18
/**
 * Neutrally aligned object. */
#define FLAG_IS_NEUTRAL 20
/**
 * Can see invisible objects. */
#define FLAG_SEE_INVISIBLE 21
/**
 * Object can be pushed. */
#define FLAG_CAN_ROLL 22
/**
 * If set and when connected object is triggered, the object will have
 * the connection reset, so it doesn't need to wait for object that
 * triggered the connection to move off. */
#define FLAG_CONNECT_RESET 23
/**
 * Object will change face with direction. */
#define FLAG_IS_TURNABLE 24
/**
 * Object is applied when left. */
#define FLAG_WALK_OFF 25
/**
 * Object is applied when flying object enters the tile. */
#define FLAG_FLY_ON 26
/**
 * Object is applied when flying object leaves the tile. */
#define FLAG_FLY_OFF 27
/**
 * The object will be removed when object::food reaches 0. */
#define FLAG_IS_USED_UP 28
/**
 * The object is identified. */
#define FLAG_IDENTIFIED 29
/**
 * Object reflects from walls (lightning, missiles). */
#define FLAG_REFLECTING 30
/**
 * Changes to other_arch when anim is done. */
#define FLAG_CHANGING 31

/* Start of values in flags[1] */

/**
 * Object splits into stats.food other objs.
 * @todo Remove? */
#define FLAG_SPLITTING 32
/**
 * Object will hit back when hit. */
#define FLAG_HITBACK 33
/**
 * Object will disappear when dropped. */
#define FLAG_STARTEQUIP 34
/**
 * Object blocks view. */
#define FLAG_BLOCKSVIEW 35
/**
 * Monster is undead. */
#define FLAG_UNDEAD 36
/**
 * The object can stack. */
#define FLAG_CAN_STACK 37
/**
 * Monster doesn't attack enemies, only if it's attacked first. */
#define FLAG_UNAGGRESSIVE 38
/**
 * Object will reflect missiles. */
#define FLAG_REFL_MISSILE 39
/**
 * Object will reflect spells. */
#define FLAG_REFL_SPELL 40
/**
 * Wizard-like spells cannot pass this tile. */
#define FLAG_NO_MAGIC 41
/**
 * fix_player() won't be called. */
#define FLAG_NO_FIX_PLAYER 42
/**
 * The object is evil. */
#define FLAG_IS_EVIL 43
/**
 * Object runs away from nearest player but can still attack
 * from distance. */
#define FLAG_RUN_AWAY 45
/**
 * Objects with can_pass_thru can pass through this object as if it
 * wasn't there. */
#define FLAG_PASS_THRU 46
/**
 * Object can pass through objects with @ref FLAG_PASS_THRU set. */
#define FLAG_CAN_PASS_THRU 47
/**
 * Outdoor tile. */
#define FLAG_OUTDOOR 48
/**
 * Item is unique. */
#define FLAG_UNIQUE 49
/**
 * Object can't be dropped. */
#define FLAG_NO_DROP 50
/**
 * The item cannot be damaged. */
#define FLAG_INDESTRUCTIBLE 51
/**
 * Monster can cast spells. */
#define FLAG_CAST_SPELL 52
/**
 * Item requires two hands to wield. */
#define FLAG_TWO_HANDED 54
/**
 * Monster can fire bows. */
#define FLAG_USE_BOW 55
/**
 * Monster can wear armour like shields, plate mails, helms, etc. */
#define FLAG_USE_ARMOUR 56
/**
 * Monster can wield weapons. */
#define FLAG_USE_WEAPON 57
/**
 * Connected object is not activated when 'pushed'. */
#define FLAG_CONNECT_NO_PUSH 58
/**
 * Connected object is not activated when 'released'. */
#define FLAG_CONNECT_NO_RELEASE 59
/**
 * Monster has a readied bow. */
#define FLAG_READY_BOW 60
/**
 * X-ray vision. */
#define FLAG_XRAYS 61
/**
 * Avoids walk_on/fly_on events for this object. */
#define FLAG_NO_APPLY 62
/**
 * The object is a floor. */
#define FLAG_IS_FLOOR 63

/* Start of values in flags[2] */

/**
 * Saves a player's life once, then destructs itself. */
#define FLAG_LIFESAVE 64
/**
 * Item is magical. */
#define FLAG_IS_MAGICAL 65
/**
 * Monster will never, ever move. */
#define FLAG_STAND_STILL 67
/**
 * Monster will move randomly. */
#define FLAG_RANDOM_MOVE 68
/**
 * Monster will evaporate if there is no enemy. */
#define FLAG_ONLY_ATTACK 69
/**
 * Allows players to pass quietly past monsters, with less chance of
 * the monsters noticing the player. */
#define FLAG_STEALTH 71
/**
 * The object is linked with other objects. */
#define FLAG_IS_LINKED 73
/**
 * The object is cursed. */
#define FLAG_CURSED 74
/**
 * The object is _very_ cursed. */
#define FLAG_DAMNED 75
/**
 * Used for floor: is the floor buildable? */
#define FLAG_IS_BUILDABLE 76
/**
 * PvP is disabled on the tile this object is on. */
#define FLAG_NO_PVP 77
/**
 * Object is designed to be thrown. */
#define FLAG_IS_THROWN 80
/**
 * Object is male. */
#define FLAG_IS_MALE 83
/**
 * Object is female. */
#define FLAG_IS_FEMALE 84
/**
 * Object is ready for use by living objects. */
#define FLAG_APPLIED 85
/**
 * Item will not be dropped from inventory. */
#define FLAG_INV_LOCKED 86
/**
 * Player has a weapon readied. */
#define FLAG_READY_WEAPON 90
/**
 * If set, won't get exp for reading the book. */
#define FLAG_NO_SKILL_IDENT 91
/**
 * If set object can see even in darkness. */
#define FLAG_SEE_IN_DARK 93
/**
 * Container can make alchemical stuff. */
#define FLAG_IS_CAULDRON 94
/**
 * Item is a powder. */
#define FLAG_DUST 95

/* Start of values in flags[3] */

/**
 * Monster can only hit once, then evaporates. */
#define FLAG_ONE_HIT 97
/**
 * Always draw the object twice. */
#define FLAG_DRAW_DOUBLE_ALWAYS 98
/**
 * Monster will attack closest living object, even friends. */
#define FLAG_BERSERK 99
/**
 * Object will never attack. */
#define FLAG_NO_ATTACK 100
/**
 * Monster can't be killed, and enemies will not consider it for attacking. */
#define FLAG_INVULNERABLE 101
/**
 * Special quest object. */
#define FLAG_QUEST_ITEM 102
/**
 * Object is trapped, ie, there is a known trap inside the object's
 * inventory. Used for map and below inventory, to mark containers
 * like corpses where player found traps. */
#define FLAG_IS_TRAPPED 103
/**
 * The object cannot be seen by anyone except DMs. */
#define FLAG_SYS_OBJECT 110
/**
 * When putting an object on map, do it exactly on position. */
#define FLAG_USE_FIX_POS 111
/**
 * Object hasn't been paid for yet. */
#define FLAG_UNPAID 112
/**
 * The object cannot be seen even with @ref FLAG_SEE_INVISIBLE. */
#define FLAG_HIDDEN 113
/**
 * Makes the wearer invisible. */
#define FLAG_MAKE_INVISIBLE 114
/**
 * Makes the wearer ethereal. */
#define FLAG_MAKE_ETHEREAL 115
/**
 * Object is a player. */
#define FLAG_IS_PLAYER 116
/**
 * Object name is "unique"- for artifacts like Stormbringer.
 *
 * Unique objects don't have a race or material (no "elven iron Stormbringer")
 * */
#define FLAG_IS_NAMED 117
/**
 * Monsters with this flag are created by spawn point and have a spawn
 * info object inside inventory. */
#define FLAG_SPAWN_MOB 118
/**
 * Objects with this flag will not be teleported by teleporters unless
 * they are in inventory of an object without this flag. */
#define FLAG_NO_TELEPORT 119
/**
 * If set, this monster will drop a corpse. */
#define FLAG_CORPSE 120
/**
 * Force a corpse, even if the object that killed the monster was too
 * high level for any experience. */
#define FLAG_CORPSE_FORCED 121
/**
 * Only players can enter the tile with object that has this flag. */
#define FLAG_PLAYER_ONLY 122
/**
 * One drop item. Used for quests, where the quest item with this flag
 * set will never drop more than once for one player. */
#define FLAG_ONE_DROP 124
/**
 * Object is permanently cursed. */
#define FLAG_PERM_CURSED 125
/**
 * Object is permanently damned. */
#define FLAG_PERM_DAMNED 126
/**
 * The object is a closer door. */
#define FLAG_DOOR_CLOSED 127
/**
 * Object is a spell. */
#define FLAG_IS_SPELL 128
/**
 * Object is a missile. */
#define FLAG_IS_MISSILE 129
/**
 * The object is shown based on its direction and the player's position. */
#define FLAG_DRAW_DIRECTION 130
/**
 * The object is drawn twice. */
#define FLAG_DRAW_DOUBLE 131
/**
 * If this and slaying field is set, the object does 2.25 times more damage
 * to race that of the 'slaying' field. */
#define FLAG_IS_ASSASSINATION 132
/**
 * Internally used from object_remove() and insert_xx(). */
#define FLAG_OBJECT_WAS_MOVED 133
/**
 * Don't save this object - remove it before we save. */
#define FLAG_NO_SAVE 134
/*@}*/

/** Should always be equal to the last defined flag. */
#define NUM_FLAGS 135
/** The number of uint32 we need to store all flags. */
#define NUM_FLAGS_32 5
/*@}*/

/** Check if object has @ref FLAG_SYS_OBJECT set. */
#define IS_SYS_INVISIBLE(__ob_) \
    QUERY_FLAG(__ob_, FLAG_SYS_OBJECT)
/** Check if the object is invisible. */
#define IS_INVISIBLE(__ob_, __player_) \
    ((QUERY_FLAG(__ob_, FLAG_SYS_OBJECT) || QUERY_FLAG(__ob_, FLAG_IS_INVISIBLE)) && ((__player_)->type != PLAYER || !CONTR((__player_))->tsi))

#define SLOW_PENALTY(xyz) ((xyz)->stats.exp)

#define EXIT_PATH(xyz) ((xyz)->slaying)
#define EXIT_X(xyz) ((xyz)->stats.hp)
#define EXIT_Y(xyz) ((xyz)->stats.sp)

/**
 * @defgroup COST_xxx Cost modes
 * Cost modes.
 *@{*/
/** Value for buying the item. */
#define COST_BUY 0
/** Value for selling the item. */
#define COST_SELL 1
/** True value of item, unadjusted. */
#define COST_TRUE 2
/*@}*/

#define DIRX(xyz) freearr_x[(xyz)->direction]
#define DIRY(xyz) freearr_y[(xyz)->direction]

#define ARMOUR_SPEED(xyz) (xyz)->last_sp
#define ARMOUR_SPELLS(xyz) (xyz)->last_heal

/**
 * @defgroup monster_movements Monster movements
 * The following definitions are for the attack_movement variable in monsters
 * if the attack_variable movement is left out of the monster archetype, or is
 * set to zero
 * the standard mode of movement from previous versions of crossfire will be
 * used. the upper four bits of movement data are not in effect when the monst
 * er has an enemy. these should only be used for non aggressive monsters.
 * to program a monsters movement add the attack movement numbers to the movem
 * ment numbers example a monster that moves in a circle until attacked and
 * then attacks from a distance:
 *                                                      CIRCLE1 = 32
 *                                              +       DISTATT = 1
 *                                      -------------------
 *                      attack_movement = 33
 * @author kholland@sunlab.cit.cornell.edu
 *@{*/
/**
 * Move toward a player if far, but maintain some space,
 * attack from a distance - good for missile users only */
#define DISTATT  1
/** Run but attack if player catches up to object */
#define RUNATT   2
/** Run to then hit player then run away cyclically */
#define HITRUN   3
/** Wait for player to approach then hit, move if hit */
#define WAITATT  4
/** Rush toward player blindly, similar to dumb monster */
#define RUSH     5
/** Always run never attack good for sim. of weak player */
#define ALLRUN   6
/** Attack from a distance if hit as recommended by Frank */
#define DISTHIT  7
/** Monster does not try to move towards player if far */
#define WAIT2    8

#define UNUSED_MOVE 16

/**
 * if the upper four bits of move_type / attack_movement
 * are set to this number, the monster will move in a
 * circle until it is attacked, or the enemy field is
 * set, this is good for non-aggressive monsters and NPC */
#define CIRCLE1 32

/** Same as above but a larger circle is used */
#define CIRCLE2 48

/**
 * The Monster will pace back and forth until attacked
 * this is HORIZONTAL movement */
#define PACEH   64

/**
 * the monster will pace as above but the length of the
 * pace area is longer and the monster stops before
 * changing directions
 * this is HORIZONTAL movement */
#define PACEH2  80

/**
 * the monster will go in a random direction until
 * it is stopped by an obstacle, then it chooses another
 * direction. */
#define RANDO   96

/** constantly move in a different random direction */
#define RANDO2 112

/**
 * The Monster will pace back and forth until attacked
 * this is VERTICAL movement */
#define PACEV  128

/**
 * the monster will pace as above but the length of the
 * pace area is longer and the monster stops before
 * changing directions
 * this is VERTICAL movement */
#define PACEV2 144

/** The monster uses waypoints (if it has any) */
#define WPOINT 176

/* bitmasks for upper and lower 4 bits from 8 bit fields */
#define LO4    15
#define HI4    240
/*@}*/

#define BLANK_FACE_NAME "blank.111"
#define NEXT_ITEM_FACE_NAME "next_item.101"
#define PREVIOUS_ITEM_FACE_NAME "prev_item.101"

/**
 * Simple function we use below to keep adding to the same string
 * but also make sure we don't overwrite that string.
 * @param dest String to append to.
 * @param orig String to append.
 * @param curlen Current length of dest. Will be updated by this function.
 * @param maxlen Maximum length of dest buffer. */
static inline void safe_strcat(char *dest, const char *orig, size_t *curlen, size_t maxlen)
{
    if (*curlen == (maxlen - 1)) {
        return;
    }

    strncpy(dest + *curlen, orig, maxlen - *curlen - 1);
    dest[maxlen - 1] = 0;
    *curlen += strlen(orig);

    if (*curlen > (maxlen - 1)) {
        *curlen = maxlen - 1;
    }
}

#define DESCRIBE_PATH(retbuf, variable, name)                        \
    if (variable)                                                    \
    {                                                                \
        int i, j = 0;                                                \
        strcat(retbuf, "(" name ": ");                               \
                                                                     \
        for (i = 0; i < NRSPELLPATHS; i++)                           \
        {                                                            \
            if (variable & (1 << i))                                 \
            {                                                        \
                if (j)                                               \
                    strcat(retbuf, ", ");                            \
                else                                                 \
                    j = 1;                                           \
                                                                     \
                strcat(retbuf, spellpathnames[i]);                   \
            }                                                        \
        }                                                            \
                                                                     \
        strcat(retbuf, ")");                                         \
    }

#define DESCRIBE_PATH_SAFE(retbuf, variable, name, len, maxlen)      \
    if (variable)                                                    \
    {                                                                \
        int i, j = 0;                                                \
        safe_strcat(retbuf, "(" name ": ", len, maxlen);             \
                                                                     \
        for (i = 0; i < NRSPELLPATHS; i++)                           \
        {                                                            \
            if (variable & (1 << i))                                 \
            {                                                        \
                if (j)                                               \
                    safe_strcat(retbuf, ", ", len, maxlen);          \
                else                                                 \
                    j = 1;                                           \
                                                                     \
                safe_strcat(retbuf, spellpathnames[i], len, maxlen); \
            }                                                        \
        }                                                            \
                                                                     \
        safe_strcat(retbuf, ")", len, maxlen);                       \
    }

/**
 * Flags for object_apply_item().
 *
 * @anchor AP_xxx */
enum apply_flag {
    /* Basic flags, always use one of these */
    AP_NULL = 0,
    AP_APPLY = 1,
    AP_UNAPPLY = 2,
    AP_BASIC_FLAGS = 15,
    /* Optional flags, for bitwise or with a basic flag */
    AP_NO_MERGE = 16,
    AP_IGNORE_CURSE = 32,
    AP_NO_EVENT = 64
};

/** Cut off point of when an object is put on the active list or not */
#define MIN_ACTIVE_SPEED 0.00001f

/**
 * @defgroup bresenham_algorithm Bresenham algorithm
 * Bresenham line drawing algorithm. Implemented as macros for
 * flexibility and speed.
 *@{*/

/**
 * Bresenham init.
 * dx & dy are input only and will not be changed.
 * All other parameters are the outputs which will be initialized */
#define BRESENHAM_INIT(dx, dy, fraction, stepx, stepy, dx2, dy2)      \
    {                                                                 \
        (dx2) = (dx) << 1;                                            \
        (dy2) = (dy) << 1;                                            \
                                                                      \
        if ((dy) < 0)                                                 \
        {                                                             \
            (dy2) = -(dy2);                                           \
            (stepy) = -1;                                             \
        }                                                             \
        else                                                          \
        {                                                             \
            (stepy) = 1;                                              \
        }                                                             \
                                                                      \
        if ((dx) < 0)                                                 \
        {                                                             \
            (dx2) = -(dx2);                                           \
            (stepx) = -1;                                             \
        }                                                             \
        else                                                          \
        {                                                             \
            (stepx) = 1;                                              \
        }                                                             \
                                                                      \
        if ((dx2) > (dy2))                                            \
            (fraction) = (dy2) - (dx) * (stepx);                      \
        else                                                          \
            (fraction) = (dx2) - (dy) * (stepy);                      \
    }

/**
 * Bresenham line stepping macro.
 * x, y are input-output and will be always be changed
 * fraction is also input-output, but should be initialized with
 * BRESENHAM_INIT.
 * stepx, stepy, dx2 and dy2 are input only and should also
 * be initialized by BRESENHAM_INIT */
#define BRESENHAM_STEP(x, y, fraction, stepx, stepy, dx2, dy2)        \
    if ((dx2) > (dy2))                                                \
    {                                                                 \
        if ((fraction) >= 0)                                          \
        {                                                             \
            (y) += (stepy);                                           \
            (fraction) -= (dx2);                                      \
        }                                                             \
                                                                      \
        (x) += (stepx);                                               \
        (fraction) += (dy2);                                          \
    }                                                                 \
    else                                                              \
    {                                                                 \
        if ((fraction) >= 0)                                          \
        {                                                             \
            (x) += (stepx);                                           \
            (fraction) -= (dy2);                                      \
        }                                                             \
                                                                      \
        (y) += (stepy);                                               \
        (fraction) += (dx2);                                          \
    }
/*@}*/

/** Name of the quest container archetype. */
#define QUEST_CONTAINER_ARCHETYPE "quest_container"

/**
 * Acquire the name of a specified quest. If the quest name is not set, uses the
 * UID of the quest.
 */
#define QUEST_NAME(_quest) \
    ((_quest)->race != NULL ? (_quest)->race : (_quest)->name)

/**
 * @defgroup quest_statuses Quest statuses
 * Quest statuses. Stored in object::magic.
 *@{*/

/** The quest has been started. */
#define QUEST_STATUS_STARTED 0
/** The quest has been completed. */
#define QUEST_STATUS_COMPLETED 1
/*@}*/

/**
 * @defgroup quest_types Quest types
 * All the possible quest types.
 *@{*/
/** Used for containers. */
#define QUEST_TYPE_NONE 0
/** The quest requires you to kill X monsters. */
#define QUEST_TYPE_KILL 1
/** The quest requires you to get item X from some location. */
#define QUEST_TYPE_ITEM 2
/**
 * The quest is automatically started by giving the player an item (one-drop
 * quest item, keys from killing a monster/opening a chest, etc). */
#define QUEST_TYPE_ITEM_DROP 3
/**
 * The quest is not handled by the server quest module; instead, it
 * is handled specially by scripts. */
#define QUEST_TYPE_SPECIAL 4
/*@}*/

/**
 * Returns the element size of an array.
 * @param arrayname The array's name.
 * @return The number of elements. */
#define arraysize(arrayname) (sizeof(arrayname) / sizeof(*(arrayname)))

/**
 * @defgroup INTERFACE_TIMEOUT_xxx Interface timeout definitions
 * Settings for interface timeout, a setting which makes NPC stop
 * movement certain amount of time when they respond to player's message.
 *
 * The length of the message affects the timeout value.
 *@{*/
/**
 * After this many characters in the message, for each X characters,
 * @ref INTERFACE_TIMEOUT_SECONDS will be added to the timeout, where X =
 * the value of this constant. */
#define INTERFACE_TIMEOUT_CHARS 50
/**
 * @see INTERFACE_TIMEOUT_CHARS */
#define INTERFACE_TIMEOUT_SECONDS 5
/**
 * Initial number of seconds for the timeout. */
#define INTERFACE_TIMEOUT_INITIAL 5
/**
 * Maximum number of seconds the NPC may stop moving for, regardless of
 * message length. */
#define INTERFACE_TIMEOUT_MAX 60 * 5
/*@}*/

/** Check if the keyword represents a true value. */
#define KEYWORD_IS_TRUE(_keyword) (!strcasecmp((_keyword), "yes") || !strcasecmp((_keyword), "on") || !strcasecmp((_keyword), "true"))
/** Check if the keyword represents a false value. */
#define KEYWORD_IS_FALSE(_keyword) (!strcasecmp((_keyword), "no") || !strcasecmp((_keyword), "off") || !strcasecmp((_keyword), "false"))

/**
 * @defgroup SPAWN_RELATIVE_LEVEL_xxx Spawn point relative levels
 * Constants used to define spawn point relative level colors.
 *@{*/
/** Green. */
#define SPAWN_RELATIVE_LEVEL_GREEN 1
/** Blue. */
#define SPAWN_RELATIVE_LEVEL_BLUE 2
/** Yellow. */
#define SPAWN_RELATIVE_LEVEL_YELLOW 3
/** Orange. */
#define SPAWN_RELATIVE_LEVEL_ORANGE 4
/** Red. */
#define SPAWN_RELATIVE_LEVEL_RED 5
/** Purple. */
#define SPAWN_RELATIVE_LEVEL_PURPLE 6
/*@}*/

/**
 * Maximum height difference between two tiles to be able to move between them.
 */
#define MOVE_MAX_HEIGHT_DIFF 12

/**
 * @defgroup FOR_MAP_LAYER_xxx Map layer looping macros
 * Macros used for looping through objects on a specific map square and layer.
 *@{*/
/**
 * The loop constructor.
 * @param _m Map.
 * @param _x X position.
 * @param _y Y position.
 * @param _layer Layer, one of @ref LAYER_xxx.
 * @param _sub_layer Which sub-layer to look for. If -1, will search all
 * sub-layers.
 * @param _obj Variable of an object pointer (does not need to be
 * initialized); will contain the found object, if any.
 * @warning Does not support stacking.
 * @note Use @ref FOR_MAP_LAYER_BREAK to break out, instead of the
 * traditional 'break'.  */
#define FOR_MAP_LAYER_BEGIN(_m, _x, _y, _layer, _sub_layer, _obj) \
    { \
        int __sub_layer; \
        object *__tmp, *__next; \
        tag_t __next_tag; \
        for (__sub_layer = ((_sub_layer) == -1 ? 0 : (_sub_layer)); __sub_layer < ((_layer) == LAYER_SYS ? 1 : ((_sub_layer) == -1 ? NUM_SUB_LAYERS : ((_sub_layer) + 1))); __sub_layer++) \
        { \
            for (__tmp = (_layer) == LAYER_SYS ? GET_MAP_OB((_m), (_x), (_y)) : GET_MAP_OB_LAYER((_m), (_x), (_y), (_layer), __sub_layer); __tmp && __tmp->layer == (_layer) && __tmp->sub_layer == __sub_layer; __tmp = __next) \
            { \
                __next = __tmp->above; \
                __next_tag = 0; \
                if (__next) \
                { \
                    __next_tag = __next->count; \
                } \
                (_obj) = __tmp;

/**
 * Break out of the loop. */
#define FOR_MAP_LAYER_BREAK \
    __sub_layer = NUM_SUB_LAYERS; \
    break; \

/**
 * End the loop. */
#define FOR_MAP_LAYER_END \
    if (__next && !OBJECT_VALID(__next, __next_tag)) \
    { \
        FOR_MAP_LAYER_BREAK; \
    } \
    } \
    } \
    }
/*@}*/

/**
 * @defgroup FOR_xxx Object looping macros
 * Macros used for looping through objects in various locations.
 * @author Andreas Kirschbaum
 *@{*/
/**
 * Constructs a loop iterating over the inventory of an object. Example:
 * <pre>
 * FOR_INV_PREPARE(op, inv)
 *     logger_print(LOG(INFO), "%s\n", inv->name);
 * FOR_INV_FINISH();
 * </pre>
 * or
 * <pre>
 * FOR_INV_PREPARE(op, inv) {
 *     logger_print(LOG(INFO), "%s\n", inv->name);
 * } FOR_INV_FINISH();
 * </pre>
 * @param op_ the object to iterate over
 * @param it_ a variable name that holds the inventory objects; a new variable
 * will be declared; modifications do not affect the loop
 */
#define FOR_INV_PREPARE(op_, it_)                               \
    do {                                                        \
        object *it_ = (op_)->inv;                               \
        FOR_OB_AND_BELOW_PREPARE(it_);
/**
 * Finishes #FOR_INV_PREPARE().
 */
#define FOR_INV_FINISH()                                        \
        FOR_OB_AND_BELOW_FINISH();                              \
    } while(0)

/**
 * Constructs a loop iterating over all objects above an object.
 * @param op_ the object to iterate over
 * @param it_ a variable name that holds the inventory objects; a new variable
 * will be declared; modifications do not affect the loop
 */
#define FOR_ABOVE_PREPARE(op_, it_)                             \
    do {                                                        \
        object *it_ = (op_)->above;                             \
        FOR_OB_AND_ABOVE_PREPARE(it_);
/**
 * Finishes #FOR_ABOVE_PREPARE().
 */
#define FOR_ABOVE_FINISH()                                      \
        FOR_OB_AND_ABOVE_FINISH();                              \
    } while(0)

/**
 * Constructs a loop iterating over all objects below an object.
 * @param op_ the object to iterate over
 * @param it_ a variable name that holds the inventory objects; a new variable
 * will be declared; modifications do not affect the loop
 */
#define FOR_BELOW_PREPARE(op_, it_)                             \
    do {                                                        \
        object *it_ = (op_)->below;                             \
        FOR_OB_AND_BELOW_PREPARE(it_);
/**
 * Finishes #FOR_BELOW_PREPARE().
 */
#define FOR_BELOW_FINISH()                                      \
        FOR_OB_AND_BELOW_FINISH();                              \
    } while(0)

/**
 * Constructs a loop iterating over all objects of a map tile.
 * @param map_ the map to iterate over
 * @param mx_ the map's x-coordinate to iterate over
 * @param my_ the map's y-coordinate to iterate over
 * @param it_ a variable name that holds the inventory objects; a new variable
 * will be declared; modifications do not affect the loop
 */
#define FOR_MAP_PREPARE(map_, mx_, my_, it_)                    \
    do {                                                        \
        object *it_ = GET_MAP_OB((map_), (mx_), (my_));         \
        FOR_OB_AND_ABOVE_PREPARE(it_);
/**
 * Finishes #FOR_MAP_PREPARE().
 */
#define FOR_MAP_FINISH()                                        \
        FOR_OB_AND_ABOVE_FINISH();                              \
    } while(0)

/**
 * Constructs a loop iterating over an object and all objects above it in the
 * same pile.
 * @param op_ the object to start with
 */
#define FOR_OB_AND_ABOVE_PREPARE(op_) FOR_OB_PREPARE(op_, above, __LINE__)
/**
 * Finishes #FOR_OB_AND_ABOVE_PREPARE().
 */
#define FOR_OB_AND_ABOVE_FINISH() FOR_OB_FINISH()

/**
 * Constructs a loop iterating over an object and all objects below it in the
 * same pile.
 * @param op_ the object to start with
 */
#define FOR_OB_AND_BELOW_PREPARE(op_) FOR_OB_PREPARE(op_, below, __LINE__)
/**
 * Finishes #FOR_OB_AND_BELOW_PREPARE().
 */
#define FOR_OB_AND_BELOW_FINISH() FOR_OB_FINISH()

/**
 * Constructs a loop iterating over a set of objects. This function should not
 * be used directly in client code.
 * @param op_ the object start start with
 * @param field_ the field that holds the "next" pointer within op_
 * @param suffix_ a suffix for constructing unique variable names
 */
#define FOR_OB_PREPARE(op_, field_, suffix_) FOR_OB_PREPARE2(op_, field_, suffix_)
/**
 * Constructs a loop iterating over a set of objects. This function should not
 * be used at all. Use FOR_OB_PREPARE() instead.
 * @param op_ the object start start with
 * @param field_ the field that holds the "next" pointer within op_
 * @param suffix_ a suffix for constructing unique variable names
 */
#define FOR_OB_PREPARE2(op_, field_, suffix_)                     \
    do {                                                        \
        object *next##suffix_ = (op_);                        \
        tag_t next_tag##suffix_ = next##suffix_ == NULL ? 0 : next##suffix_->count;\
        while (((op_) = next##suffix_) != NULL) {              \
            if (was_destroyed(next##suffix_, next_tag##suffix_)) {\
                break;                                          \
            }                                                   \
            next##suffix_ = next##suffix_->field_;                               \
            next_tag##suffix_ = next##suffix_ == NULL ? 0 : next##suffix_->count;
/**
 * Finishes #FOR_OB_PREPARE().
 */
#define FOR_OB_FINISH()                                         \
        }                                                       \
    } while(0)

/*@}*/

#endif
