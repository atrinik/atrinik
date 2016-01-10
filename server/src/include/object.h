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
 * Object related structures, core of Atrinik
 */

#ifndef OBJECT_H
#define OBJECT_H

#include <decls.h>
#include <attack.h>

/**
 * Get the weight of an object. If the object is a container or doesn't
 * have nrof, include the weight it is carrying.
 */
#define WEIGHT(op) (!op->nrof || op->type == CONTAINER ? op->weight + op->carrying : op->weight)

#define WEIGHT_NROF(op, nrof) (MAX(1, nrof) * op->weight + op->carrying)

/**
 * @defgroup MOVE_APPLY_xxx move_apply() function call flags
 */
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
 *
 */
#define MOVE_APPLY_VANISHED 32
/** move_apply() called from saving function */
#define MOVE_APPLY_SAVING   64
/*@}*/

/**
 * @defgroup CHECK_WALK_xxx WALK ON/OFF function return flags
 */
/*@{*/
#define CHECK_WALK_OK        0
#define CHECK_WALK_DESTROYED 1
#define CHECK_WALK_MOVED     2
/*@}*/

/**
 * This structure allows any object to have extra fields the Flex loader
 * does not know about.
 */
typedef struct key_value {
    /** Name of this extra field. Shared string. */
    shstr *key;

    /** Value of this extra field. Shared string. */
    shstr *value;

    /** Next value in the list. */
    struct key_value *next;
} key_value_t;

/**
 * Object structure.
 */
struct obj {
    /* These variables are not changed by object_copy(): */

    /**
     * Next object in the 'active' list
     * This is used in process_events
     * so that the entire object list does not
     * need to be gone through.
     */
    struct obj *active_next;

    /**
     * Previous object in the 'active' list
     * This is used in process_events
     * so that the entire object list does not
     * need to be gone through.
     */
    struct obj *active_prev;

    /** Pointer to the object stacked below this one */
    struct obj *below;

    /**
     * Pointer to the object stacked above this one
     * Note: stacked in the *same* environment
     */
    struct obj *above;

    /** Pointer to the first object in the inventory */
    struct obj *inv;

    /**
     * Pointer to the object which is the environment.
     * This is typically the container that the object is in.
     * If env == NULL then the object is on a map or in the nirvana.
     */
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
     * errors.
     */
    uint32_t damage_round_tag;

    /**
     * How much weight this object contains (of objects in inv). This
     * is not copied by memcpy(), since the memcpy() doesn't actually
     * copy over the inventory either, so it would create bogus carrying
     * weight in some cases.
     */
    uint32_t carrying;

    /** Type-dependant extra data. */
    void *custom_attrset;

    /* These get an extra add_refcount(), after having been copied by memcpy().
     * All fields below this point are automatically copied by memcpy. If
     * adding something that needs a refcount updated, make sure you modify
     * object_copy() to do so. */

    /** The name of the object, obviously... */
    const char *name;

    /** Of foo, etc */
    const char *title;

    /** Human, goblin, dragon, etc */
    const char *race;

    /**
     * Which race to do double damage to.
     * If this is an exit, this is the filename
     */
    const char *slaying;

    /** If this is a book/sign/magic mouth/etc */
    const char *msg;

    /** Artifact name that was applied to this object by
     * give_artifact_abilities(). */
    shstr *artifact;

    /** Custom name assigned by player. */
    shstr *custom_name;

    /** Glow color. */
    shstr *glow;

    /** Monster/player to follow even if not closest */
    struct obj *enemy;

    /** This object starts to attack us! Only player & monster */
    struct obj *attacked_by;

    /**
     * Pointer to the object which controls this one.
     *
     * Owner should not be referred to directly - get_owner() should be
     * used instead.
     */
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
    struct archetype *arch;

    /** Pointer used for various things */
    struct archetype *other_arch;

    /** Items to be generated */
    struct treasureliststruct *randomitems;

    /** Struct pointer to the 'face' - the picture(s) */
    New_Face *face;

    /** Struct pointer to the inventory 'face' - the picture(s) */
    New_Face *inv_face;

    /** How much money it is worth (or contains) */
    int64_t value;

    /** flags matching events of event objects inside object ->inv */
    uint32_t event_flags;

    /** Attributes of the object - the weight */
    uint32_t weight;

    /**
     * Weight-limit of object - player and container should have this...
     * perhaps we can substitute it?
     */
    uint32_t weight_limit;

    /** Paths the object is attuned to */
    uint32_t path_attuned;

    /** Paths the object is repelled from */
    uint32_t path_repelled;

    /** Paths the object is denied access to */
    uint32_t path_denied;

    /** How many of the objects */
    uint32_t nrof;

    /** This is used from map2 update! */
    uint32_t update_tag;

    /** Various flags */
    uint32_t flags[NUM_FLAGS_32];

    /** X position in the map for this object */
    int16_t x;

    /** Y position in the map for this object */
    int16_t y;

    /**
     * Z-Position in the map (in pixels) for this object.
     *
     * For floor (layer 1), this makes the client use tile stretching. All
     * other objects get Y position (height) adjustment on the map (100 = the
     * object moves 100 pixels further to the top, -50 = the object moves
     * 50 pixels to the bottom).
     */
    int16_t z;

    /** Needed to target the nearest enemy */
    int16_t attacked_by_distance;

    /** The damage sent with map2 */
    int16_t last_damage;

    /**
     * type flags for different environment (tile is under water, firewalk,...)
     * A object which can be applied GIVES this terrain flags to his owner
     */
    uint16_t terrain_type;

    /** The object can move over/is unaffected from this terrain type */
    uint16_t terrain_flag;

    /** What materials this object consists of */
    uint16_t material;

    /** This holds the real material value like what kind of steel */
    int16_t material_real;

    /** Last healed. Depends on constitution */
    int16_t last_heal;

    /** As last_heal, but for spell points */
    int16_t last_sp;

    /** as last_sp, except for grace */
    int16_t last_grace;

    /** How long since we last ate */
    int16_t last_eat;

    /** An index into the animation array */
    uint16_t animation_id;

    /** An index into the animation array for the client inv */
    uint16_t inv_animation_id;

    /**
     * X align of the object on the actual map. Similar to object::z,
     * a value of 100 = object is moved 100 pixels to the right, -50 and
     * the object is moved 50 pixels to the left.
     */
    int16_t align;

    /** Object's rotation value in degrees. */
    int16_t rotate;

    /** Object is a light source */
    int8_t glow_radius;

    /** Any magical bonuses to this item */
    int8_t magic;

    /** How the object was last drawn (animation) */
    uint8_t state;

    /** the level of this object (most used for mobs & player) */
    int8_t level;

    /** Means the object is moving that way. */
    int8_t direction;

    /**
     * quick pos is 0 for single arch, xxxx0000 for a head
     * or x/y offset packed to 4 bits for a tail
     * warning: change this when include > 15x15 monster
     */
    uint8_t quick_pos;

    /** PLAYER, BULLET, etc. See define.h */
    uint8_t type;

    /** Sub type definition - this will be sent to client too */
    uint8_t sub_type;

    /** Quality of an item in range from 0-100 */
    uint8_t item_quality;

    /** Condition of repair of an item - from 0 to 100% item_quality */
    uint8_t item_condition;

    /** Item crafted from race x. "orcish xxx", "dwarven xxxx" */
    uint8_t item_race;

    /** Level needed to use or apply this item */
    uint8_t item_level;

    /** if set and item_level, item_level in this skill is needed */
    uint8_t item_skill;

    /** What stage in attack mode */
    int8_t move_status;

    /** What kind of movement */
    uint8_t move_type;

    /** What kind of attack movement */
    uint8_t attack_move_type;

    /** Combination of @ref anim_flags "animation flags". */
    uint8_t anim_flags;

    /** Animation speed in ticks */
    uint8_t anim_speed;

    /** Last animated tick. */
    uint8_t last_anim;

    /** Various @ref BEHAVIOR_xxx "behavior flags". */
    uint8_t behavior;

    /** Monster runs away if its hp goes below this percentage. */
    uint8_t run_away;

    /** the layer in a map, this object will be sorted in */
    uint8_t layer;

    /** Sub layer. */
    uint8_t sub_layer;

    /** Quickslot ID this object goes in */
    uint8_t quickslot;

    /**
     * Chance to block a blow.
     */
    uint8_t block;

    /**
     * Amount of damage that is absorbed by this item.
     */
    uint8_t absorb;

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
     * the power to boost items to 100%+.
     */
    uint8_t attack[NROFATTACKS];

    /** Resistance against attacks in % - range from -125 to 125 */
    int8_t protection[NROFATTACKS];

    /** Power rating of the object. */
    int8_t item_power;

    /**
     * How much to zoom the object horizontally.
     *
     * 0 = 100 = 100% zoom of the object, which means the original (no
     * actual zooming is done). 50 = 50% of the original object's size,
     * 200 = 200% of the original object's size.
     */
    int16_t zoom_x;

    /**
     * How much to zoom the object vertically.
     *
     * 0 = 100 = 100% zoom of the object, which means the original (no
     * actual zooming is done). 50 = 50% of the original object's size,
     * 200 = 200% of the original object's size.
     */
    int16_t zoom_y;

    /** Object's alpha value. */
    uint8_t alpha;

    /** Object's glowing speed. */
    uint8_t glow_speed;

    /** The overall speed of this object */
    double speed;

    /** How much speed is left to spend this round */
    double speed_left;

    /** new weapon speed system. swing of weapon */
    double weapon_speed;

    /** Weapon speed left */
    double weapon_speed_left;

    /** Str, Con, Dex, etc */
    living stats;

    /** Fields not explicitly known by the loader. */
    key_value_t *key_values;
};

/** Used to link together several objects. */
struct oblnk {

    /** The object link. */
    union {
        /** Link. */
        struct oblnk *link;

        /** Object. */
        object *ob;
    } objlink;

    /** Next object in this list. */
    struct oblnk *next;

    /** Previous object in this list. */
    struct oblnk *prev;

    /** Object ID. */
    tag_t id;

    /** Used as connected value in buttons/gates. */
    long value;
};

#define free_objectlink_simple(_chunk_) mempool_return(pool_objectlink, (_chunk_));

#define CONTR(ob) ((player *) ((ob)->custom_attrset))

/* This returns TRUE if the object is something that
 * should be displayed in the look window */
#define LOOK_OBJ(_ob) (!IS_SYS_INVISIBLE(_ob) && _ob->type != PLAYER)

/**
 * @defgroup UP_OBJ_xxx Object update flags
 *
 * Used by update_object() to know if the object being passed is
 * being added or removed.
 */
/*@{*/
/** Object was inserted in a map */
#define UP_OBJ_INSERT   1
/** Object was removed from a map tile */
#define UP_OBJ_REMOVE   2
/**
 * Critical object flags has been changed, rebuild tile flags but NOT
 * increase tile counter
 */
#define UP_OBJ_FLAGS    3
/** Only thing that changed was the face */
#define UP_OBJ_FACE     4
/** Update flags & face (means increase tile update counter */
#define UP_OBJ_FLAGFACE 5
/** Force full update */
#define UP_OBJ_ALL      6
/*@}*/

/**
 * Macro for the often used object validity test (verify a pointer/count
 * pair)
 */
#define OBJECT_VALID(_ob_, _count_) ((_ob_) && (_ob_)->count == ((tag_t) _count_) && !QUERY_FLAG((_ob_), FLAG_REMOVED) && !OBJECT_FREE(_ob_))

/** Test the object is not removed nor freed - but no count test */
#define OBJECT_ACTIVE(_ob_) (!QUERY_FLAG((_ob_), FLAG_REMOVED) && !OBJECT_FREE(_ob_))

/** Test if an object  */
#define OBJECT_FREE(_ob_) ((_ob_)->count == 0)

/**
 * @defgroup INS_xxx Object insertion flags.
 *
 * These are flags passed to object_insert_map() and object_insert_into().
 * Note that all flags may not be meaningful for both functions.
 */
/*@{*/
/** Don't try to merge inserted object with ones already on space. */
#define INS_NO_MERGE 1
/**
 * Don't call check_walk_on against the originator - saves CPU
 * time if you know the inserted object is not meaningful in
 * terms of having an effect.
 */
#define INS_NO_WALK_ON 2
/**
 * Fall through to the bottommost map. Will also take care of updating the
 * object's sub-layer to be that of the floor tile with the highest Z.
 */
#define INS_FALL_THROUGH 4
/*@}*/

/**
 * @defgroup REMOVAL_xxx Object removal flags.
 * Flags used for object_remove()>@
 *@{*/
/**
 * Do not adjust weight.
 */
#define REMOVE_NO_WEIGHT 1
/**
 * Do not perform walk off check.
 */
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
/**
 * Use exits.
 */
#define BEHAVIOR_EXITS 0x08
/**
 * Use secret passages.
 */
#define BEHAVIOR_SECRET_PASSAGES 0x10
/**
 * Guard behavior.
 */
#define BEHAVIOR_GUARD 0x20
/*@}*/

/** Decrease an object by one. */
#define decrease_ob(xyz) object_decrease(xyz, 1)

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
 * in infravision mode.
 */
#define CORPSE_INFRAVISION_PERCENT 75

/**
 * Returns the head part of an object. For single-tile objects returns the
 * object itself.
 *
 * @param op
 * The object.
 * @return
 * The head object.
 */
#define HEAD(op) ((op)->head ? (op)->head : (op))

/**
 * Structure used for object::custom_attrset of magic mirrors.
 */
typedef struct magic_mirror_struct {
    /** Map the magic mirror is pointing to. */
    struct mapdef *map;

    /** X position on magic_mirror_struct::map that should be mirrored. */
    int16_t x;

    /** Y position on magic_mirror_struct::map that should be mirrored. */
    int16_t y;
} magic_mirror_struct;

/**
 * Returns the ::magic_mirror_struct that holds the magic mirror's map, x
 * and y. Can be NULL in case of a magic mirror that is only used for zooming
 * or similar effect, and not mirroring.
 */
#define MMIRROR(ob) ((magic_mirror_struct *) ((ob)->custom_attrset))

/**
 * Check whether the specified object can talk.
 */
#define OBJECT_CAN_TALK(ob) ((ob)->type == MONSTER && ((ob)->msg || \
        HAS_EVENT((ob), EVENT_SAY)) && !OBJECT_VALID((ob)->enemy, \
        (ob)->enemy_count))

/**
 * Check whether an object is cursed/damned.
 */
#define OBJECT_CURSED(ob) (QUERY_FLAG((ob), FLAG_CURSED) || QUERY_FLAG((ob), FLAG_DAMNED))

/**
 * @deprecated
 */
#define OBJ_DESTROYED_BEGIN(_op) \
    do { \
        tag_t __tag_ ## _op = (_op)->count;
/**
 * @deprecated
 */
#define OBJ_DESTROYED(_op) (!OBJECT_VALID((_op), __tag_ ## _op))
/**
 * @deprecated
 */
#define OBJ_DESTROYED_END() \
    } while (0)

/**
 * Check if the specified object has been destroyed.
 *
 * @param obj
 * The object to check.
 * @param tag
 * Tag to check against.
 */
#define OBJECT_DESTROYED(obj, tag) \
    (OBJECT_FREE(obj) || (obj)->count != (tag))

/**
 * @defgroup OBJECTS_DESTROYED_xxx Destroyed objects check
 * Macro that tracks and checks the destroyed state of multiple objects
 * simultaneously.
 *
 * Example usage:
 * @code
 * OBJECTS_DESTROYED_BEGIN(op, hitter) {
 *     // Do something with op and/or hitter
 *     if (OBJECTS_DESTROYED_ANY(op, hitter)) {
 *         // End processing
 *         return;
 *     }
 *
 *     // Do something more with op and/or hitter
 * } OBJECTS_DESTROYED_END();
 * @endcode
 *@{*/
/**
 * Helper macro for OBJECTS_DESTROYED_BEGIN(); used internally.
 */
#define _OBJECTS_DESTROYED_DEFINE(obj)      \
    HARD_ASSERT(!OBJECT_FREE(obj));         \
    tag_t __tag_##obj = obj->count;
/**
 * Helper macro for OBJECTS_DESTROYED(); used internally.
 */
#define _OBJECTS_DESTROYED(obj) \
    (OBJECT_DESTROYED(obj, __tag_##obj))
/**
 * Helper macro for OBJECTS_DESTROYED_ANY(); used internally.
 */
#define _OBJECTS_DESTROYED_ANY(obj) \
    _OBJECTS_DESTROYED(obj) ||
/**
 * Helper macro for OBJECTS_DESTROYED_ALL(); used internally.
 */
#define _OBJECTS_DESTROYED_ALL(obj) \
    _OBJECTS_DESTROYED(obj) &&

/**
 * Begin tracking objects for destruction checks. This creates a new scope,
 * which MUST be closed with OBJECTS_DESTROYED_END().
 *
 * @param ... The objects to track.
 */
#define OBJECTS_DESTROYED_BEGIN(...) \
    do { \
        FOR_EACH(_OBJECTS_DESTROYED_DEFINE, __VA_ARGS__)
/**
 * Check if any of the objects tracked by OBJECTS_DESTROYED_BEGIN() have
 * been destroyed.
 *
 * When specifying all of the objects that were tracked by a previous use
 * of OBJECTS_DESTROYED_BEGIN(), it is recommended that they be in the same
 * order.
 *
 * @param ...
 * The objects to check. MUST be one (or more) of the objects that were
 * tracked by a previous OBJECTS_DESTROYED_BEGIN(). The specified order
 * does not have to be the same as the one defined previously for
 * OBJECTS_DESTROYED_BEGIN().
 */
#define OBJECTS_DESTROYED_ANY(...) \
    (FOR_EACH(_OBJECTS_DESTROYED_ANY, __VA_ARGS__) 0)
/**
 * Check if all of the objects tracked by OBJECTS_DESTROYED_BEGIN() have
 * been destroyed.
 *
 * When specifying all of the objects that were tracked by a previous use
 * of OBJECTS_DESTROYED_BEGIN(), it is recommended that they be in the same
 * order.
 *
 * @param ...
 * The objects to check. MUST be one (or more) of the objects that were
 * tracked by a previous OBJECTS_DESTROYED_BEGIN(). The specified order
 * does not have to be the same as the one defined previously for
 * OBJECTS_DESTROYED_BEGIN().
 */
#define OBJECTS_DESTROYED_ALL(...) \
    (FOR_EACH(_OBJECTS_DESTROYED_ALL, __VA_ARGS__) 1)
/**
 * Check if the specified object tracked by a previous use of
 * OBJECTS_DESTROYED_BEGIN() has been destroyed.
 *
 * It is preferable to use this macro when checking a single object,
 * instead of using OBJECTS_DESTROYED_ALL/OBJECTS_DESTROYED_ANY with a
 * single argument.
 *
 * @param obj
 * The object to check.
 */
#define OBJECTS_DESTROYED(obj) \
    (_OBJECTS_DESTROYED(obj))
/**
 * Closes the scope created by OBJECTS_DESTROYED_BEGIN().
 */
#define OBJECTS_DESTROYED_END() \
    } while (0)
/*@}*/

/**
 * Check whether the object is a flying projectile.
 */
#define OBJECT_IS_PROJECTILE(ob) (QUERY_FLAG((ob), FLAG_FLYING) && (QUERY_FLAG((ob), FLAG_IS_MISSILE) || QUERY_FLAG((ob), FLAG_IS_SPELL)))

/**
 * Check whether the object is a ranged weapon.
 */
#define OBJECT_IS_RANGED(_ob) ((_ob)->type == WAND || (_ob)->type == ROD || (_ob)->type == BOW || (_ob)->type == SPELL || (_ob)->type == SKILL || ((_ob)->type == ARROW && QUERY_FLAG((_ob), FLAG_IS_THROWN)))

/**
 * Check whether the object is ammunition (quiver, arrow, bolt, etc).
 */
#define OBJECT_IS_AMMO(_ob) (((_ob)->type == CONTAINER && \
        (_ob)->race != NULL && (_ob)->sub_type == ST1_CONTAINER_QUIVER) || \
        ((_ob)->type == ARROW && !QUERY_FLAG((_ob), FLAG_IS_THROWN)))

/* Prototypes */
extern object *active_objects;
extern const char *gender_noun[GENDER_MAX];
extern const char *gender_subjective[GENDER_MAX];
extern const char *gender_subjective_upper[GENDER_MAX];
extern const char *gender_objective[GENDER_MAX];
extern const char *gender_possessive[GENDER_MAX];
extern const char *gender_reflexive[GENDER_MAX];
extern int freearr_x[SIZEOFFREE];
extern int freearr_y[SIZEOFFREE];
extern int maxfree[SIZEOFFREE];
extern int freedir[SIZEOFFREE];
extern const char *object_flag_names[NUM_FLAGS + 1];

bool
object_can_merge(object *ob1, object *ob2);
object *
object_merge(object *op);
uint32_t
object_weight_sum(object *op);
void
object_weight_add(object *op, uint32_t weight);
void
object_weight_sub(object *op, uint32_t weight);
object *
object_get_env(object *op);
bool
object_is_in_inventory(const object *op, const object *inv);
void
object_dump(const object *op, StringBuffer *sb);
void
object_dump_rec(const object *op, StringBuffer *sb);
void
object_owner_clear(object *op);
void
object_owner_set(object *op, object *owner);
void
object_owner_copy(object *op, object *clone_ob);
object *
object_owner(object *op);
void
object_copy(object *op, const object *src, bool no_speed);
void
object_copy_full(object *op, const object *src);
void
object_init(void);
void
object_deinit(void);
object *
object_get(void);
void
object_update_turnable(object *op);
void
object_update_speed(object *op);
void
object_update(object *op, int action);
void
object_drop_inventory(object *op);
void
object_destroy_inv(object *ob);
void
object_destroy(object *ob);
void
object_destruct(object *op);
void
object_remove(object *op, int flags);
object *
object_insert_map(object *op, mapstruct *m, object *originator, int flag);
object *
object_stack_get(object *op, uint32_t nrof);
object *
object_stack_get_reinsert(object *op, uint32_t nrof);
object *
object_stack_get_removed(object *op, uint32_t nrof);
object *
object_decrease(object *op, uint32_t i);
object *
object_insert_into(object *op, object *where, int flag);
object *
object_find_arch(object *op, archetype_t *at);
object *
object_find_type(object *op, uint8_t type);
int
object_dir_to_target(object *op, object *target);
bool
object_can_pick(const object *op, const object *item);
object *
object_clone(const object *op);
object *
object_load_str(const char *obstr);
void
object_free_key_values(object *op);
key_value_t *
object_get_key_link(const object *op, shstr *key);
shstr *
object_get_value(const object *op, const char *const key);
bool
object_set_value(object *op, const char *key, const char *value, bool add_key);
int
object_matches_string(object *op, object *caller, const char *str);
int
object_get_gender(const object *op);
void
object_reverse_inventory(object *op);
bool
object_enter_map(object    *op,
                 object    *exit,
                 mapstruct *m,
                 int        x,
                 int        y,
                 bool       fixed_pos);
const char *
object_get_str(const object *op);
char *
object_get_str_r(const object *op, char *buf, size_t bufsize);
int
object_blocked(object *op, mapstruct *m, int x, int y);
object *
object_create_singularity(const char *name);
void
object_save(const object *op, FILE *fp);

/**
 * Returns the owner of the specified object. If there is no object,
 * the originally passed object pointer is returned instead.
 *
 * @param op
 * The object.
 * @return
 * Owner of the object (may be the same object).
 */
static inline object *
OWNER (object *op)
{
    HARD_ASSERT(op != NULL);

    object *owner = object_owner(op);
    if (owner != NULL) {
        return owner;
    }

    return op;
}

#endif
