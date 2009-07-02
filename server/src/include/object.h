/************************************************************************
*            Atrinik, a Multiplayer Online Role Playing Game            *
*                                                                       *
*                     Copyright (C) 2009 Alex Tokar                     *
*                                                                       *
* Fork from Daimonin (Massive Multiplayer Online Role Playing Game)     *
* and Crossfire (Multiplayer game for X-windows).                       *
*                                                                       *
* This program is free software; you can redistribute it and/or modify  *
* it under the terms of the GNU General Public License as published by  *
* the Free Software Foundation; either version 3 of the License, or     *
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

#ifndef OBJECT_H
#define OBJECT_H

#ifdef WIN32
#pragma pack(push,1)
#endif

/* move_apply() function call flags */
#define MOVE_APPLY_DEFAULT	0
#define MOVE_APPLY_WALK_ON	1
#define MOVE_APPLY_FLY_ON	2
#define MOVE_APPLY_WALK_OFF 4
#define MOVE_APPLY_FLY_OFF	8
#define MOVE_APPLY_MOVE		16 /* means: our object makes a step in/out of this tile */
#define MOVE_APPLY_VANISHED	32 /* when a player logs out, the player char not "move" out of a tile
                                * but it "turns to nothing on the spot". This sounds senseless but for
								* example a move out can trigger a teleporter action. This flag prevents
								* a loging out/exploding object is teleported after removing it from the spot.
								*/
#define MOVE_APPLY_SAVING	64 /* move_apply() called from saving function */

/* WALK ON/OFF function return flags */
#define CHECK_WALK_OK		 0
#define CHECK_WALK_DESTROYED 1
#define CHECK_WALK_MOVED	 2

/* i sorted the members of this struct in 4 byte (32 bit) groups. This will help compiler
 * and cpu to make aligned access of the members, and can (and will) make things smaller
 * and faster - but this depends on compiler & system too. */
typedef struct obj
{
	/* These variables are not changed by copy_object(): */

	/* Next & previous object in the 'active' */
	struct obj *active_next;

	/* List.  This is used in process_events
	 * so that the entire object list does not
	 * need to be gone through. */
	struct obj *active_prev;

	/* Pointer to the object stacked below this one */
	struct obj *below;

	/* Pointer to the object stacked above this one
	 * Note: stacked in the *same* environment */
	struct obj *above;

	/* Pointer to the first object in the inventory */
	struct obj *inv;

	/* Pointer to the object which is the environment.
	 * This is typically the container that the object is in.
	 * if env == NULL then the object is on a map or in the nirvana. */
	struct obj *env;

	/* Pointer to the rest of a large body of objects */
	struct obj *more;

	/* Points to the main object of a large body */
	struct obj *head;

	/* Pointer to the map in which this object is present */
	struct mapdef *map;

	/* Which nr. of object created this is. */
	/* hmmm... unchanged? this count should be new
	 * set every time a freed object is used again
	 * this count refers the logical object. MT. */
	tag_t count;

    /* These get an extra add_refcount(), after having been copied by memcpy().
     * All fields beow this point are automatically copied by memcpy.  If
     * adding something that needs a refcount updated, make sure you modify
     * copy_object to do so.  Everything below here also gets cleared
     * by clear_object() */

	/* The name of the object, obviously... */
	const char *name;

	/* Of foo, etc */
	const char *title;

	/* human, goblin, dragon, etc */
	const char *race;

	/* Which race to do double damage to.
	 * If this is an exit, this is the filename */
	const char *slaying;

	/* If this is a book/sign/magic mouth/etc */
	const char *msg;

	/* here starts copy_object() releated data */

	/* these are some internals */

	/* Pointer to archetype */
	struct archt *arch;

	/* Items to be generated */
	struct treasureliststruct *randomitems;

	/* we can remove chosen_skill & exp_obj by drop here a uint8 with a list of skill
	 * numbers. Mobs has no skill and player can grab it from player struct. For exp,
	 * i will use skill numbers in golems/ammo and spell objects. So, this can be removed. */

	/* the skill chosen to use */
	struct obj *chosen_skill;

	/* the exp. obj (category) assoc. w/ this object */
	struct obj *exp_obj;

	/* flags matching events of event objects inside object ->inv */
	uint32 event_flags;

	/* needed when we want chain script direct without browsing
	 * the objects inventory (this is needed when we want mutiple
	 * scripts of same kind in one object). */
	/*struct obj *event_ptr;*/

	/* now "real" object releated data */

	/* Pointer used for various things */
	struct archt *other_arch;

	/* struct ptr to the 'face' - the picture(s) */
	New_Face *face;

	/* struct ptr to the inventory 'face' - the picture(s) */
	New_Face *inv_face;

	/* Attributes of the object - the weight */
	sint32 weight;

	/* Weight-limit of object - player and container should have this... perhaps we can substitute it?*/
	uint32 weight_limit;

	/* How much weight this object contains (of objects in inv) */
	sint32 carrying;

	/* Paths the object is attuned to */
	uint32 path_attuned;

	/* Paths the object is repelled from */
	uint32 path_repelled;

	/* Paths the object is denied access to */
	uint32 path_denied;

	/* How much money it is worth (or contains) */
	sint32 value;

	/* How many of the objects */
	uint32 nrof;

	/* needed for the damage info for client in map2 */
	uint32 damage_round_tag;

	/* this is used from map2 update! */
	uint32 update_tag;

	/* various flags */
	uint32 flags[NUM_FLAGS_32];

	/* What count the enemy has */
	tag_t enemy_count;

	/* Monster/player to follow even if not closest */
	struct obj *enemy;

	/* the tag of attacker, so we can be sure */
	tag_t attacked_by_count;

	/* This object start to attack us! only player & monster */
	struct obj *attacked_by;

	/* What count the owner had (in case owner has been freed) */
	tag_t ownercount;

	/* Pointer to the object which controls this one
	 * Owner should not be referred to directly
	 * - get_owner() should be used instead. */
	struct obj *owner;

	/* *map is part of "object head" but this not? hmm */

	/* X-Position in the map for this object */
	sint16 x;

	/* Y-Position in the map for this object */
	sint16 y;

	/* needed to target the nearest enemy */
	sint16 attacked_by_distance;

	/* thats the damage send with map2 */
	uint16 last_damage;

	/* type flags for different enviroment (tile is under water, firewalk,...)
	 * A object which can be applied GIVES this terrain flags to his owner */
	uint16 terrain_type;

	/* The object can move over/is unaffected from this terrain type */
	uint16 terrain_flag;

	/* What materials this object consist of */
	uint16 material;

	/* This hold the real material value like what kind of steel */
	sint16 material_real;

	/* Last healed. Depends on constitution */
	sint16 last_heal;

	/* As last_heal, but for spell points */
	sint16 last_sp;

	/* as last_sp, except for grace */
	sint16 last_grace;

	/* How long since we last ate */
	sint16 last_eat;

	/* An index into the animation array */
    uint16 animation_id;

	/* An index into the animation array for the client inv */
    uint16 inv_animation_id;

	/* object is a light source */
	sint8 glow_radius;

	/* some stuff for someone coming softscrolling / smooth animations */
	/*sint8 tile_xoff;*/			/* x-offset of position of an object inside a tile */
	/*sint8 tile_yoff;*/			/* same for y-offset */

	/* Any magical bonuses to this item */
	sint8 magic;

	/* How the object was last drawn (animation) */
	uint8 state;

	/* the level of this object (most used for mobs & player) */
	sint8 level;

	/* Means the object is moving that way. */
	sint8 direction;

	/* Object is oriented/facing that way. */
	sint8 facing;

	/* quick pos is 0 for single arch, xxxx0000 for a head
	 * or x/y offset packed to 4 bits for a tail
	 * warning: change this when include > 15x15 monster */
	uint8 quick_pos;

	/* PLAYER, BULLET, etc.  See define.h */
	uint8 type;

	/* sub type definition - this will be send to client too */
	uint8 sub_type1;

	/* quality of a item in range from 0-100 */
	uint8 item_quality;

	/* condition of repair of an item - from 0 to 100% item_quality */
	uint8 item_condition;

	/* item crafted from race x. "orcish xxx", "dwarven xxxx" */
	uint8 item_race;

	/* level needed to use or apply this item */
	uint8 item_level;

	/* if set and item_level, item_level in this skill is needed */
	uint8 item_skill;

	/* What stage in attack mode */
	sint8 move_status;

	/* What kind of attack movement */
    uint8 move_type;

	/* special shadow variable: show dir to targeted enemy */
	sint8 anim_enemy_dir;

	/* sic: shows moving dir or -1 when object do something else */
	sint8 anim_moving_dir;

	/* if we change facing in movement, we must test for update the anim*/
	sint8 anim_enemy_dir_last;

	/* sic:*/
	sint8 anim_moving_dir_last;

	/* the last direction this monster was facing */
	sint8 anim_last_facing;

	/* the last direction this monster was facing backbuffer */
	sint8 anim_last_facing_last;

	/* animation speed in ticks */
    uint8 anim_speed;

	/* ticks between animation-frames */
	uint8 last_anim;

	/* See crossfire.doc */
	uint8 will_apply;

	/* Monster runs away if it's hp goes below this percentage. */
	uint8 run_away;

	/* pickup mode - See crossfire.doc */
	uint8 pick_up;

	/* The object is hidden. We don't use a flag here because
	 * the range from 0-255 tells us the quality of the hide */
	uint8 hide;

	/* the layer in a map, this object will be sorted in */
	uint8 layer;

	/* Intrinsic resist against damage - range from -125 to +125 */
	sint8 resist[NROFATTACKS];

	/* our attack values - range from 0%-125%. (negative values makes no sense).
	 * Note: we can in theory allow 300% damage for a attacktype.
	 * all we need is to increase sint8 to sint16. Thats true for
	 * resist & protection too. But it will be counter the damage
	 * calculation. Think about a way a player deals only 10 damage
	 * at base but can grab so many items that he does 3000% damage.
	 * thats not how this should work. More damage should come from
	 * the stats.dmg value - NOT from this source.
	 * The "125% max border" should work nice and the 25% over 100%
	 * should give a little boost. I think about to give player crafters
	 * the power to boost items to 100%+. */
	uint8 attack[NROFATTACKS];

	/* Resistance against attacks in % - range from 125-125*/
	sint8 protection[NROFPROTECTIONS];

	/* The overall speed of this object */
	float speed;

	/* How much speed is left to spend this round */
	float speed_left;

	/* new weapon speed system. swing of weapon */
	float weapon_speed;

	float weapon_speed_left;

	float weapon_speed_add;

	/* object stats like hp, sp, grace ... */
	living stats;

	/* REMOVE IS IN PROCESS */
	uint32 attacktype;


#ifdef POSITION_DEBUG
	/* For debugging: Where it was last inserted */
	sint16 ox, oy;
#endif

	/* Type-dependant extra data. */
    void *custom_attrset;

	/* Quickslot ID this object goes in */
	uint8 quickslot;
} object;

#ifdef WIN32
#pragma pack(pop)
#endif

/* Used to link together several objects */
typedef struct oblnk {
  	object *ob;

  	struct oblnk *next;

  	tag_t id;
} objectlink;

/* Used to link together several object links */
typedef struct oblinkpt {
  	struct oblnk *link;

	/* Used as connected value in buttons/gates */
  	long value;

  	struct oblinkpt *next;
} oblinkpt;

extern object *active_objects;

#define CONTR(ob) ((player *)((ob)->custom_attrset))

/* This returns TRUE if the object is somethign that
 * should be displayed in the look window */
#define LOOK_OBJ(_ob) (!IS_SYS_INVISIBLE(_ob) && _ob->type!=PLAYER)

/* Used by update_object to know if the object being passed is
 * being added or removed. */
#define UP_OBJ_INSERT   1	/* object was inserted in a map */
#define UP_OBJ_REMOVE   2   /* object was removed from a map tile */
#define UP_OBJ_FLAGS    3   /* critical object flags has been changed, rebuild tile flags but NOT increase tile counter */
#define UP_OBJ_FACE     4   /* Only thing that changed was the face */
#define UP_OBJ_FLAGFACE 5   /* update flags & face (means increase tile update counter */
#define UP_OBJ_ALL		6   /* force full update */
#define UP_OBJ_LAYER	7   /* object layer was changed, rebuild layer systen - used from invisible for example */

/* Macro for the often used object validity test (verify an pointer/count pair) */
#define OBJECT_VALID(_ob_, _count_) ((_ob_) && (_ob_)->count == ((tag_t)_count_) && !QUERY_FLAG((_ob_), FLAG_REMOVED) && !OBJECT_FREE(_ob_))

/* Test the object is not removed nor freed - but no count test */
#define OBJECT_ACTIVE(_ob_) (!QUERY_FLAG((_ob_),FLAG_REMOVED) && !OBJECT_FREE(_ob_))

/* Test if an object is in the free-list */
#define OBJECT_FREE(_ob_) ((_ob_)->count==0 && CHUNK_FREE(_ob_))

/* These are flags passed to insert_ob_in_map and
 * insert_ob_in_ob.  Note that all flags may not be meaningful
 * for both functions. */
#define INS_NO_MERGE		0x0001
#define INS_NO_WALK_ON		0x0002
#define INS_TAIL_MARKER		0x0004 /* used intern from insert_xx to track multi
                                    * arch problems - don't use!
									*/

/* Pooling memory management stuff */
#define MEMPOOL_TRACKING /* Enable tracking/freeing of mempools ? */

/*#define MEMPOOL_OBJECT_TRACKING*/ /* enable a global list of *all* objects
                                 * we have allocated. We can browse them to
								 * control & debug them. WARNING: Enabling this
								 * feature will slow down the server *EXTREMLY* and should
								 * only be done in real debug runs
								 */

/* Minimalistic memory management data for a single chunk of memory
 * It is (currently) up to the application to keep track of which pool
 * it belongs to. */
struct mempool_chunk {
    /* This struct must always be padded for longword alignment of the data coming behind it.
     * Not a problem as long as we only keep a single pointer here, but be careful
     * if adding more data. */

	/* Used for the free list and the limbo list. NULL if this
	 * memory chunk has been allocated and is in use */
    struct mempool_chunk *next;

#ifdef MEMPOOL_OBJECT_TRACKING
	/* for debug only */
	struct mempool_chunk *obj_prev;

	/* for debug only */
	struct mempool_chunk *obj_next;

	uint32 flags;

	/* to what mpool is this memory part related? */
	uint32 pool_id;

	/* the REAL unique ID number */
	uint32 id;
#endif
};

/* Optional constructor to be called when expanding */
typedef void (* chunk_constructor) (void *ptr);

/* Optional destructor to be called when freeing */
typedef void (* chunk_destructor) (void *ptr);

/* Data for a single memory pool */
struct mempool {
	/* First free chunk */
    struct mempool_chunk *first_free;

	/* How many chunks to allocate at each expansion */
    uint32 expand_size;

	/* size of chunks, excluding sizeof(mempool_chunk) and padding */
    uint32 chunksize;

	/* List size counters */
    uint32 nrof_used, nrof_free;

	/* Optional constructor to be called when getting chunks */
    chunk_constructor constructor;

	/* Optional destructor to be called when returning chunks */
    chunk_destructor destructor;

	/* Description of chunks. Mostly for debugging */
    char *chunk_description;

	/* Spacial handling flags. See definitions below */
    uint32 flags;

    struct puddle_info *first_puddle_info;
};

#ifdef MEMPOOL_TRACKING
struct puddle_info {
    struct puddle_info *next;

    struct mempool_chunk *first_chunk;

    /* Local freelist only for this puddle. Temporary used when freeing memory*/
    struct mempool_chunk *first_free, *last_free;

    uint32 nrof_free;
};
#endif

typedef enum {
#ifdef MEMPOOL_TRACKING
    POOL_PUDDLE,
#endif
    POOL_OBJECT,
    POOL_PLAYER,
    NROF_MEMPOOLS
} mempool_id;

/* Get the memory management struct for a chunk of memory */
#define MEM_POOLDATA(ptr) (((struct mempool_chunk *)(ptr)) - 1)

/* Get the actual user data area from a mempool reference */
#define MEM_USERDATA(ptr) ((void *)(((struct mempool_chunk *)(ptr)) + 1))

/* Check that a chunk of memory is in the free (or removed for objects) list */
#define CHUNK_FREE(ptr) (MEM_POOLDATA(ptr)->next != NULL)

#define MEMPOOL_ALLOW_FREEING 1 /* Allow puddles from this pool to be freed */
#define MEMPOOL_BYPASS_POOLS  2 /* Don't use pooling, but only malloc/free instead */

extern struct mempool mempools[];

#endif
