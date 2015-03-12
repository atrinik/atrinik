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
 * Object related structures, core of Atrinik */

#ifndef OBJECT_H
#define OBJECT_H

/**
 * Get the weight of an object. If the object is a container or doesn't
 * have nrof, include the weight it is carrying. */
#define WEIGHT(op) (!op->nrof || op->type == CONTAINER ? op->weight + op->carrying : op->weight)

#define WEIGHT_NROF(op, nrof) ((nrof ? op->weight * (sint32) nrof : op->weight) + op->carrying)

/**
 * @defgroup MOVE_APPLY_xxx move_apply() function call flags */
/*@{*/
#define MOVE_APPLY_DEFAULT  0
#define MOVE_APPLY_WALK_ON  1
#define MOVE_APPLY_FLY_ON   2
#define MOVE_APPLY_WALK_OFF 4
#define MOVE_APPLY_FLY_OFF  8
/** Our object makes a step in/out of this tile */
#define MOVE_APPLY_MOVE     16
/**
 * When a player logs out, the player char not "move" out of a tile
 * but it "turns to nothing on the spot". This sounds senseless but for
 * example a move out can trigger a teleporter action. This flag prevents
 * a logging out/exploding object is teleported after removing it from the spot.
 * */
#define MOVE_APPLY_VANISHED 32
/** move_apply() called from saving function */
#define MOVE_APPLY_SAVING   64
/*@}*/

/**
 * @defgroup CHECK_WALK_xxx WALK ON/OFF function return flags */
/*@{*/
#define CHECK_WALK_OK        0
#define CHECK_WALK_DESTROYED 1
#define CHECK_WALK_MOVED     2
/*@}*/

/**
 * This structure allows any object to have extra fields the Flex loader
 * does not know about. */
typedef struct key_value_struct {
    /** Name of this extra field. Shared string. */
    const char *key;

    /** Value of this extra field. Shared string. */
    const char *value;

    /** Next value in the list. */
    struct key_value_struct *next;
} key_value;

/**
 * Object structure. */
typedef struct obj {
    /* These variables are not changed by copy_object(): */

    /**
     * Next object in the 'active' list
     * This is used in process_events
     * so that the entire object list does not
     * need to be gone through. */
    struct obj *active_next;

    /**
     * Previous object in the 'active' list
     * This is used in process_events
     * so that the entire object list does not
     * need to be gone through. */
    struct obj *active_prev;

    /** Pointer to the object stacked below this one */
    struct obj *below;

    /**
     * Pointer to the object stacked above this one
     * Note: stacked in the *same* environment */
    struct obj *above;

    /** Pointer to the first object in the inventory */
    struct obj *inv;

    /**
     * Pointer to the object which is the environment.
     * This is typically the container that the object is in.
     * If env == NULL then the object is on a map or in the nirvana. */
    struct obj *env;

    /** Pointer to the rest of a large body of objects */
    struct obj *more;

    /** Points to the main object of a large body */
    struct obj *head;

    /** Pointer to the map in which this object is present */
    struct mapdef *map;

    /** Unique object number for this object */
    tag_t count;

    /**
     * Needed for the damage info for client in map2. Also used for
     * unmodified carrying weight of magical containers to prevent rounding
     * errors. */
    uint32 damage_round_tag;

    /**
     * How much weight this object contains (of objects in inv). This
     * is not copied by memcpy(), since the memcpy() doesn't actually
     * copy over the inventory either, so it would create bogus carrying
     * weight in some cases. */
    sint32 carrying;

    /* These get an extra add_refcount(), after having been copied by memcpy().
     * All fields below this point are automatically copied by memcpy. If
     * adding something that needs a refcount updated, make sure you modify
     * copy_object to do so. */

    /** The name of the object, obviously... */
    const char *name;

    /** Of foo, etc */
    const char *title;

    /** Human, goblin, dragon, etc */
    const char *race;

    /**
     * Which race to do double damage to.
     * If this is an exit, this is the filename */
    const char *slaying;

    /** If this is a book/sign/magic mouth/etc */
    const char *msg;

    /** Artifact name that was applied to this object by
     * give_artifact_abilities(). */
    shstr *artifact;

    /** Custom name assigned by player. */
    shstr *custom_name;

    /** Monster/player to follow even if not closest */
    struct obj *enemy;

    /** This object starts to attack us! Only player & monster */
    struct obj *attacked_by;

    /**
     * Pointer to the object which controls this one.
     *
     * Owner should not be referred to directly - get_owner() should be
     * used instead. */
    struct obj *owner;

    /** The skill chosen to use */
    struct obj *chosen_skill;

    /** The exp. obj (category) associated with this object */
    struct obj *exp_obj;

    /** What count the enemy has */
    tag_t enemy_count;

    /** What count the owner had (in case owner has been freed) */
    tag_t ownercount;

    /** The tag of attacker, so we can be sure */
    tag_t attacked_by_count;

    /** Pointer to archetype */
    struct archt *arch;

    /** Pointer used for various things */
    struct archt *other_arch;

    /** Items to be generated */
    struct treasureliststruct *randomitems;

    /** Struct pointer to the 'face' - the picture(s) */
    New_Face *face;

    /** Struct pointer to the inventory 'face' - the picture(s) */
    New_Face *inv_face;

    /** How much money it is worth (or contains) */
    sint64 value;

    /** flags matching events of event objects inside object ->inv */
    uint32 event_flags;

    /** Attributes of the object - the weight */
    sint32 weight;

    /**
     * Weight-limit of object - player and container should have this...
     * perhaps we can substitute it? */
    uint32 weight_limit;

    /** Paths the object is attuned to */
    uint32 path_attuned;

    /** Paths the object is repelled from */
    uint32 path_repelled;

    /** Paths the object is denied access to */
    uint32 path_denied;

    /** How many of the objects */
    uint32 nrof;

    /** This is used from map2 update! */
    uint32 update_tag;

    /** Various flags */
    uint32 flags[NUM_FLAGS_32];

    /** X position in the map for this object */
    sint16 x;

    /** Y position in the map for this object */
    sint16 y;

    /**
     * Z-Position in the map (in pixels) for this object.
     *
     * For floor (layer 1), this makes the client use tile stretching. All
     * other objects get Y position (height) adjustment on the map (100 = the
     * object moves 100 pixels further to the top, -50 = the object moves
     * 50 pixels to the bottom). */
    sint16 z;

    /** Needed to target the nearest enemy */
    sint16 attacked_by_distance;

    /** The damage sent with map2 */
    uint16 last_damage;

    /**
     * type flags for different environment (tile is under water, firewalk,...)
     * A object which can be applied GIVES this terrain flags to his owner */
    uint16 terrain_type;

    /** The object can move over/is unaffected from this terrain type */
    uint16 terrain_flag;

    /** What materials this object consists of */
    uint16 material;

    /** This holds the real material value like what kind of steel */
    sint16 material_real;

    /** Last healed. Depends on constitution */
    sint16 last_heal;

    /** As last_heal, but for spell points */
    sint16 last_sp;

    /** as last_sp, except for grace */
    sint16 last_grace;

    /** How long since we last ate */
    sint16 last_eat;

    /** An index into the animation array */
    uint16 animation_id;

    /** An index into the animation array for the client inv */
    uint16 inv_animation_id;

    /**
     * X align of the object on the actual map. Similar to object::z,
     * a value of 100 = object is moved 100 pixels to the right, -50 and
     * the object is moved 50 pixels to the left. */
    sint16 align;

    /** Object's rotation value in degrees. */
    sint16 rotate;

    /** Object is a light source */
    sint8 glow_radius;

    /** Any magical bonuses to this item */
    sint8 magic;

    /** How the object was last drawn (animation) */
    uint8 state;

    /** the level of this object (most used for mobs & player) */
    sint8 level;

    /** Means the object is moving that way. */
    sint8 direction;

    /**
     * quick pos is 0 for single arch, xxxx0000 for a head
     * or x/y offset packed to 4 bits for a tail
     * warning: change this when include > 15x15 monster */
    uint8 quick_pos;

    /** PLAYER, BULLET, etc. See define.h */
    uint8 type;

    /** Sub type definition - this will be sent to client too */
    uint8 sub_type;

    /** Quality of an item in range from 0-100 */
    uint8 item_quality;

    /** Condition of repair of an item - from 0 to 100% item_quality */
    uint8 item_condition;

    /** Item crafted from race x. "orcish xxx", "dwarven xxxx" */
    uint8 item_race;

    /** Level needed to use or apply this item */
    uint8 item_level;

    /** if set and item_level, item_level in this skill is needed */
    uint8 item_skill;

    /** What stage in attack mode */
    sint8 move_status;

    /** What kind of movement */
    uint8 move_type;

    /** What kind of attack movement */
    uint8 attack_move_type;

    /** Combination of @ref anim_flags "animation flags". */
    uint8 anim_flags;

    /** Animation speed in ticks */
    uint8 anim_speed;

    /** Last animated tick. */
    uint8 last_anim;

    /** Various @ref BEHAVIOR_xxx "behavior flags". */
    uint8 behavior;

    /** Monster runs away if its hp goes below this percentage. */
    uint8 run_away;

    /** the layer in a map, this object will be sorted in */
    uint8 layer;

    /** Sub layer. */
    uint8 sub_layer;

    /** Quickslot ID this object goes in */
    uint8 quickslot;

    /**
     * our attack values - range from 0%-125%. (negative values makes no sense).
     * Note: we can in theory allow 300% damage for a attacktype.
     * all we need is to increase sint8 to sint16. That's true for
     * resist & protection too. But it will be counter the damage
     * calculation. Think about a way a player deals only 10 damage
     * at base but can grab so many items that he does 3000% damage.
     * that's not how this should work. More damage should come from
     * the stats.dmg value - NOT from this source.
     * The "125% max border" should work nice and the 25% over 100%
     * should give a little boost. I think about to give player crafters
     * the power to boost items to 100%+. */
    uint8 attack[NROFATTACKS];

    /** Resistance against attacks in % - range from -125 to 125 */
    sint8 protection[NROFATTACKS];

    /** Power rating of the object. */
    sint8 item_power;

    /**
     * How much to zoom the object horizontally.
     *
     * 0 = 100 = 100% zoom of the object, which means the original (no
     * actual zooming is done). 50 = 50% of the original object's size,
     * 200 = 200% of the original object's size. */
    sint16 zoom_x;

    /**
     * How much to zoom the object vertically.
     *
     * 0 = 100 = 100% zoom of the object, which means the original (no
     * actual zooming is done). 50 = 50% of the original object's size,
     * 200 = 200% of the original object's size. */
    sint16 zoom_y;

    /** Object's alpha value. */
    uint8 alpha;

    /** The overall speed of this object */
    float speed;

    /** How much speed is left to spend this round */
    float speed_left;

    /** new weapon speed system. swing of weapon */
    float weapon_speed;

    /** Weapon speed left */
    float weapon_speed_left;

    /** Str, Con, Dex, etc */
    living stats;

    /** Type-dependant extra data. */
    void *custom_attrset;

    /** Fields not explicitly known by the loader. */
    key_value *key_values;
} object;

/** Used to link together several objects. */
typedef struct oblnk {

    /** The object link. */
    union {
        /** Link. */
        struct oblnk *link;

        /** Object. */
        object *ob;

        /** Ban. */
        struct ban_struct *ban;
    } objlink;

    /** Next object in this list. */
    struct oblnk *next;

    /** Previous object in this list. */
    struct oblnk *prev;

    /** Object ID. */
    tag_t id;

    /** Used as connected value in buttons/gates. */
    long value;
} objectlink;

#define free_objectlink_simple(_chunk_) mempool_return(pool_objectlink, (_chunk_));

#define CONTR(ob) ((player *) ((ob)->custom_attrset))

/* This returns TRUE if the object is something that
 * should be displayed in the look window */
#define LOOK_OBJ(_ob) (!IS_SYS_INVISIBLE(_ob) && _ob->type != PLAYER)

/**
 * @defgroup UP_OBJ_xxx Object update flags
 *
 * Used by update_object() to know if the object being passed is
 * being added or removed. */
/*@{*/
/** Object was inserted in a map */
#define UP_OBJ_INSERT   1
/** Object was removed from a map tile */
#define UP_OBJ_REMOVE   2
/**
 * Critical object flags has been changed, rebuild tile flags but NOT
 * increase tile counter */
#define UP_OBJ_FLAGS    3
/** Only thing that changed was the face */
#define UP_OBJ_FACE     4
/** Update flags & face (means increase tile update counter */
#define UP_OBJ_FLAGFACE 5
/** Force full update */
#define UP_OBJ_ALL      6
/**
 * Object layer was changed, rebuild layer systen - used from invisible
 * for example */
#define UP_OBJ_LAYER    7
/*@}*/

/**
 * Macro for the often used object validity test (verify a pointer/count
 * pair) */
#define OBJECT_VALID(_ob_, _count_) ((_ob_) && (_ob_)->count == ((tag_t) _count_) && !QUERY_FLAG((_ob_), FLAG_REMOVED) && !OBJECT_FREE(_ob_))

/** Test the object is not removed nor freed - but no count test */
#define OBJECT_ACTIVE(_ob_) (!QUERY_FLAG((_ob_), FLAG_REMOVED) && !OBJECT_FREE(_ob_))

/** Test if an object  */
#define OBJECT_FREE(_ob_) ((_ob_)->count == 0)

/**
 * @defgroup INS_xxx Object insertion flags.
 *
 * These are flags passed to insert_ob_in_map() and
 * insert_ob_in_ob(). Note that all flags may not be meaningful
 * for both functions. */
/*@{*/
/** Don't try to merge inserted object with ones already on space. */
#define INS_NO_MERGE 1
/**
 * Don't call check_walk_on against the originator - saves CPU
 * time if you know the inserted object is not meaningful in
 * terms of having an effect. */
#define INS_NO_WALK_ON 2
/*@}*/

/**
 * @defgroup REMOVAL_xxx Object removal flags.
 * Flags used for object_remove()>@
 *@{*/
/**
 * Do not adjust weight. */
#define REMOVE_NO_WEIGHT 1
/**
 * Do not perform walk off check. */
#define REMOVE_NO_WALK_OFF 2
/*@}*/

/**
 * @defgroup BEHAVIOR_xxx Behavior flags
 * These control what behavior the monster can do.
 *@{*/
/**
 * The monster will look for other friendly monsters to cast friendly spells
 * on.
 */
#define BEHAVIOR_SPELL_FRIENDLY 0x01
/**
 * The monster can open doors.
 */
#define BEHAVIOR_OPEN_DOORS 0x02
/**
 * The monster prefers low-light tiles when finding a path to its destination.
 */
#define BEHAVIOR_STEALTH 0x04
/*@}*/

/** Decrease an object by one. */
#define decrease_ob(xyz) decrease_ob_nr(xyz, 1)

/**
 * @defgroup GENDER_xxx Gender IDs.
 * IDs of the various genders.
 *@{*/
/** Neuter: no gender. */
#define GENDER_NEUTER 0
/** Male. */
#define GENDER_MALE 1
/** Female. */
#define GENDER_FEMALE 2
/** Hermaphrodite: both genders. */
#define GENDER_HERMAPHRODITE 3
/** Total number of genders. */
#define GENDER_MAX 4
/*@}*/

/**
 * Value in percent of time above which the corpse will be highlighted
 * in infravision mode. */
#define CORPSE_INFRAVISION_PERCENT 75

/**
 * Returns the head part of an object. For single-tile objects returns the
 * object itself.
 * @param op The object.
 * @return The head object. */
#define HEAD(op) ((op)->head ? (op)->head : (op))

/**
 * Structure used for object::custom_attrset of magic mirrors. */
typedef struct magic_mirror_struct {
    /** Map the magic mirror is pointing to. */
    struct mapdef *map;

    /** X position on magic_mirror_struct::map that should be mirrored. */
    sint16 x;

    /** Y position on magic_mirror_struct::map that should be mirrored. */
    sint16 y;
} magic_mirror_struct;

/**
 * Returns the ::magic_mirror_struct that holds the magic mirror's map, x
 * and y. Can be NULL in case of a magic mirror that is only used for zooming
 * or similar effect, and not mirroring. */
#define MMIRROR(ob) ((magic_mirror_struct *) ((ob)->custom_attrset))

/**
 * Check whether the specified object can talk. */
#define OBJECT_CAN_TALK(ob) ((ob)->type == MONSTER && ((ob)->msg || HAS_EVENT((ob), EVENT_SAY)))

/**
 * Check whether an object is cursed/damned. */
#define OBJECT_CURSED(ob) (QUERY_FLAG((ob), FLAG_CURSED) || QUERY_FLAG((ob), FLAG_DAMNED))

#define OBJ_DESTROYED_BEGIN(_op) \
    { \
        tag_t __tag_ ## _op = (_op)->count;
#define OBJ_DESTROYED(_op) (!OBJECT_VALID((_op), __tag_ ## _op))
#define OBJ_DESTROYED_END(_op) \
    }

/**
 * Check whether the object is a flying projectile. */
#define OBJECT_IS_PROJECTILE(ob) (QUERY_FLAG((ob), FLAG_FLYING) && (QUERY_FLAG((ob), FLAG_IS_MISSILE) || QUERY_FLAG((ob), FLAG_IS_SPELL)))

/**
 * Check whether the object is a ranged weapon. */
#define OBJECT_IS_RANGED(_ob) ((_ob)->type == WAND || (_ob)->type == ROD || (_ob)->type == BOW || (_ob)->type == SPELL || (_ob)->type == SKILL || ((_ob)->type == ARROW && QUERY_FLAG((_ob), FLAG_IS_THROWN)))

/**
 * Check whether the object is ammunition (quiver, arrow, bolt, etc).
 */
#define OBJECT_IS_AMMO(_ob) (((_ob)->type == CONTAINER && \
        (_ob)->race != NULL && (_ob)->sub_type == ST1_CONTAINER_QUIVER) || \
        ((_ob)->type == ARROW && !QUERY_FLAG((_ob), FLAG_IS_THROWN)))

#endif
