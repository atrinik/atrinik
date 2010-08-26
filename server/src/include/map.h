/************************************************************************
*            Atrinik, a Multiplayer Online Role Playing Game            *
*                                                                       *
*    Copyright (C) 2009-2010 Alex Tokar and Atrinik Development Team    *
*                                                                       *
* Fork from Daimonin (Massive Multiplayer Online Role Playing Game)     *
* and Crossfire (Multiplayer game for X-windows).                       *
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
 * The mapstruct is allocated each time a new map is opened.
 * It contains pointers (very indirectly) to all objects on the map. */

#ifndef MAP_H
#define MAP_H

/** Number of darkness level. Add +1 for "total dark" */
#define MAX_DARKNESS 7

int global_darkness_table[MAX_DARKNESS + 1];
int map_tiled_reverse[TILED_MAPS];

/** For exit objects: this is a player unique map */
#define MAP_PLAYER_MAP 1

/**
 * @defgroup LAYER_xxx Layer types
 * The layer types used for different objects.
 *@{*/
/** System objects. */
#define LAYER_SYS 0
/** Floor. */
#define LAYER_FLOOR 1
/** Floor masks. */
#define LAYER_FMASK 2
/** Items: weapons, armour, books, etc. */
#define LAYER_ITEM 3
/** Another layer for items, often decoration. */
#define LAYER_ITEM2 4
/** Walls. */
#define LAYER_WALL 5
/** Living objects like players and monsters. */
#define LAYER_LIVING 6
/** Spell effects. */
#define LAYER_EFFECT 7
/*@}*/

/**
 * The number of object layers. */
#define NUM_LAYERS 7

/**
 * @defgroup map_struct_macros Map structure macros
 * These macros are used to access flags of a map, the map reset time,
 * the map owner, etc.
 *@{*/

/** This is when the map will reset */
#define MAP_WHEN_RESET(m)      ((m)->reset_time)
/** The map reset timeout */
#define MAP_RESET_TIMEOUT(m)   ((m)->reset_timeout)
/** The map difficulty */
#define MAP_DIFFICULTY(m)      ((m)->difficulty)
/** The map timeout */
#define MAP_TIMEOUT(m)         ((m)->timeout)
/** The map swap time */
#define MAP_SWAP_TIME(m)       ((m)->swap_time)
/** Is the map outdoors map? */
#define MAP_OUTDOORS(m)        ((m)->map_flags & MAP_FLAG_OUTDOOR)
/** Is the map unique map? */
#define MAP_UNIQUE(m)          ((m)->map_flags & MAP_FLAG_UNIQUE)
/** Does the map have fixed reset time? */
#define MAP_FIXED_RESETTIME(m) ((m)->map_flags & MAP_FLAG_FIXED_RTIME)
/** Is the map no-save map? */
#define MAP_NOSAVE(m)          ((m)->map_flags & MAP_FLAG_NO_SAVE)
/** Does the map disallow magic? */
#define MAP_NOMAGIC(m)         ((m)->map_flags & MAP_FLAG_NOMAGIC)
/** Does the map disallow priest spells? */
#define MAP_NOPRIEST(m)        ((m)->map_flags & MAP_FLAG_NOPRIEST)
/** Does the map disallow harmful spells? */
#define MAP_NOHARM(m)          ((m)->map_flags & MAP_FLAG_NOHARM)
/** Does the map disallow summoning spells? */
#define MAP_NOSUMMON(m)        ((m)->map_flags & MAP_FLAG_NOSUMMON)
/** Is the map a fixed login map? */
#define MAP_FIXEDLOGIN(m)      ((m)->map_flags & MAP_FLAG_FIXED_LOGIN)
/** Are players unable to save on this map? */
#define MAP_PLAYER_NO_SAVE(m) ((m)->map_flags & MAP_FLAG_PLAYER_NO_SAVE)
/** Is the map PVP area? */
#define MAP_PVP(m)             ((m)->map_flags & MAP_FLAG_PVP)
/** Darkness of a map */
#define MAP_DARKNESS(m)        (m)->darkness
/** Width of a map */
#define MAP_WIDTH(m)           (m)->width
/** Height of a map */
#define MAP_HEIGHT(m)          (m)->height
/**
 * Convenience function - total number of spaces on map is used in many
 * places. */
#define MAP_SIZE(m)            ((m)->width * (m)->height)
/** Enter X position of a map */
#define MAP_ENTER_X(m)         (m)->enter_x
/** Enter Y position of a map */
#define MAP_ENTER_Y(m)         (m)->enter_y
/*@}*/

/**
 * @defgroup map_load_flags Map loading flags
 * Flags passed to map loading functions such as ready_map_name() and
 * load_original_map().
 *@{*/

/**
 * Always load map from the map directory, and don't do unique items or
 * the like. */
#define MAP_FLUSH         0x1
/**
 * This map is player-specific. Don't do any more name translation on
 * it. */
#define MAP_PLAYER_UNIQUE 0x2
/** Unused. */
#define MAP_BLOCK         0x4
/** Active objects shouldn't be put on active list. */
#define MAP_STYLE         0x8
/** Unused. */
#define MAP_ARTIFACT      0x20
/** Indicates that the name string is a shared string */
#define MAP_NAME_SHARED   0x40
/** Original map. Generate treasures */
#define MAP_ORIGINAL      0x80
/*@}*/

/**
 * @defgroup map_memory_flags Map memory flags
 * Map memory flags, used from @ref mapstruct::in_memory.
 *@{*/

/** Map is fully loaded. */
#define MAP_IN_MEMORY   1
/** Map spaces have been saved to disk. */
#define MAP_SWAPPED     2
/** This map is being loaded. */
#define MAP_LOADING     3
/** Map being saved. Will stop remove_ob() from some processing. */
#define MAP_SAVING      4
/*@}*/

/* new macros for map layer system */
#define GET_MAP_SPACE_PTR(M_, X_, Y_) \
	(&((M_)->spaces[(X_) + (M_)->width * (Y_)]))

#define GET_MAP_SPACE_FIRST(M_) \
	((M_)->first)
#define GET_MAP_SPACE_LAST(M_) \
	((M_)->last)
#define GET_MAP_SPACE_LAYER(M_, L_) \
	((M_)->layer[L_])

#define SET_MAP_SPACE_FIRST(M_, O_) \
	((M_)->first = (O_))
#define SET_MAP_SPACE_LAST(M_, O_) \
	((M_)->last = (O_))
#define SET_MAP_SPACE_LAYER(M_, L_, O_) \
	((M_)->layer[L_] = (O_))

#define GET_MAP_UPDATE_COUNTER(M, X, Y) \
	((M)->spaces[(X) + (M)->width * (Y)].update_tile)

#define INC_MAP_UPDATE_COUNTER(M, X, Y) \
	((M)->spaces[((X) + (M)->width * (Y))].update_tile++)

#define GET_MAP_MOVE_FLAGS(M, X, Y) \
	((M)->spaces[(X) + (M)->width * (Y)].move_flags)
#define SET_MAP_MOVE_FLAGS(M, X, Y, C) \
	((M)->spaces[(X) + (M)->width * (Y)].move_flags = C)
#define GET_MAP_FLAGS(M, X, Y) \
	((M)->spaces[(X) + (M)->width * (Y)].flags)
#define SET_MAP_FLAGS(M, X, Y, C) \
	((M)->spaces[(X) + (M)->width * (Y)].flags = C)
#define GET_MAP_LIGHT(M, X, Y) \
	((M)->spaces[(X) + (M)->width * (Y)].light)
#define SET_MAP_LIGHT(M, X, Y, L) \
	((M)->spaces[(X) + (M)->width * (Y)].light = (sint8) L)

#define GET_MAP_OB(M, X, Y) \
	((M)->spaces[(X) + (M)->width * (Y)].first)
#define GET_MAP_OB_LAST(M, X, Y) \
	((M)->spaces[(X) + (M)->width * (Y)].last)
#define GET_MAP_OB_LAYER(_M_, _X_, _Y_, _Z_) \
	((_M_)->spaces[(_X_) + (_M_)->width * (_Y_)].layer[_Z_])
#define get_map_ob GET_MAP_OB

#define SET_MAP_OB(M, X, Y, tmp) \
	((M)->spaces[(X) + (M)->width * (Y)].first = (tmp))
#define SET_MAP_OB_LAST(M, X, Y, tmp) \
	((M)->spaces[(X) + (M)->width * (Y)].last = (tmp))
#define SET_MAP_OB_LAYER(_M_, _X_, _Y_, _Z_, tmp) \
	((_M_)->spaces[(_X_) + (_M_)->width * (_Y_)].layer[_Z_] = (tmp))
#define set_map_ob SET_MAP_OB

#define SET_MAP_DAMAGE(M, X, Y, tmp) \
	((M)->spaces[(X) + (M)->width * (Y)].last_damage = (uint16) (tmp))
#define GET_MAP_DAMAGE(M, X, Y) \
	((M)->spaces[(X) + (M)->width * (Y)].last_damage)

#define SET_MAP_RTAG(M, X, Y, tmp) \
	((M)->spaces[(X) + (M)->width * (Y)].round_tag = (uint32) (tmp))
#define GET_MAP_RTAG(M, X, Y) \
	((M)->spaces[(X) + (M)->width * (Y)].round_tag)

#define GET_BOTTOM_MAP_OB(O) ((O)->map ? GET_MAP_OB((O)->map, (O)->x, (O)->y) : NULL)

/**
 * @defgroup map_face_flags Map face flags
 * These are the 'face flags' we grab out of the flags object structure
 * 1:1.
 *
 * We use a macro to get them from the object, doing it fast AND to mask the bigger
 * object flags to uint8.
 *@{*/

/** Object is sleeping */
#define FFLAG_SLEEP     0x01
/** Object is confused */
#define FFLAG_CONFUSED  0x02
/** Object is paralyzed */
#define FFLAG_PARALYZED 0x04
/** Object is scared - it will run away */
#define FFLAG_SCARED    0x08
/** Object is blinded */
#define FFLAG_BLINDED   0x10
/** Object is invisible (can be seen with "see invisible" on) */
#define FFLAG_INVISIBLE 0x20
/** Object is etheral */
#define FFLAG_ETHEREAL  0x40
/** Object is probed */
#define FFLAG_PROBE     0x80
/*@}*/

/**
 * You should really know what you are doing before using this - you
 * should almost always be using get_map_from_coord() instead, which
 * takes into account map tiling. */
#define OUT_OF_REAL_MAP(M, X, Y) \
	((X) < 0 || (Y) < 0 || (X) >= (M)->width || (Y) >= (M)->height)

/**
 * @defgroup map_look_flags Map look flags
 * These are used in the MapLook flags element.  They are not used in
 * in the object flags structure.
 *@{*/

/** Blocks the view. */
#define P_BLOCKSVIEW          0x01
/** Spells (some) can't pass this object */
#define P_NO_MAGIC            0x02
/** Nothing can pass (wall() is true) */
#define P_NO_PASS             0x04
/** There is one or more player on this tile */
#define P_IS_PLAYER           0x08
/** Something alive is on this space */
#define P_IS_ALIVE            0x10
/** No clerical spells cast here */
#define P_NO_CLERIC           0x20
/**
 * Only players are allowed to enter this space. This excludes mobs
 * and golems but also spell effects and thrown / fired items.
 * It works like a no_pass for players only (pass_thru doesn't work for
 * it). */
#define P_PLAYER_ONLY         0x40
/**
 * A closed door is blocking this space - if we want to approach, we must
 * first check if it's possible to open it. */
#define P_DOOR_CLOSED         0x80
/**
 * We have something like an inventory checker in this tile node. */
#define P_CHECK_INV           0x100
/**
 * PvP is not possible on this tile. */
#define P_NO_PVP              0x200
/**
 * Same as NO_PASS - but objects with PASS_THRU set can cross it.
 *
 * @note If a node has NO_PASS and P_PASS_THRU set, there are 2 objects
 * in the node, one with pass_thru and one with real no_pass - then
 * no_pass will overrule pass_thru */
#define P_PASS_THRU           0x400
/**
 * We have a magic ear on this map tile... Later we should add a map
 * pointer where we attach as chained list this stuff - no search
 * or flags are then needed. */
#define P_MAGIC_EAR           0x800
/** For moving objects and what happens when they enter */
#define P_WALK_ON             0x1000
/** For moving objects and what happens when they leave */
#define P_WALK_OFF            0x2000
/** For flying objects and what happens when they leave */
#define P_FLY_OFF             0x4000
/** For flying objects and what happens when they enter */
#define P_FLY_ON              0x8000
/** There is a @ref MAGIC_MIRROR object on this tile. */
#define P_MAGIC_MIRROR 0x10000
/** Unused. */
#define P_UNUSED2 0x20000
/**
 * Of course not set for map tiles but from blocked_xx() function where
 * the get_map_from_coord() fails to grab a valid map or tile. */
#define P_OUT_OF_MAP          0x4000000
/** Skip the layer update, do flags only */
#define P_FLAGS_ONLY          0x8000000
/** If set, update the flags by looping the map objects */
#define P_FLAGS_UPDATE        0x10000000
/** Resort the layer when updating */
#define P_NEED_UPDATE         0x20000000
/**
 * Purely temporary - if set, update_position
 * does not complain if the flags are different. */
#define P_NO_ERROR            0x40000000
/**
 * Do <b>NOT</b> use this with SET_MAP_FLAGS(). This is just to mark for
 * return values of blocked(). */
#define P_NO_TERRAIN          0x80000000
/*@}*/

/** Single tile on a map */
typedef struct MapSpace_s
{
	/** Start of the objects on this map tile */
	object *first;

	/** Array of visible layer objects + for invisible (*2) */
	object *layer[NUM_LAYERS * 2];

	/** Last object in this list */
	object *last;

	/** Used to create chained light source list. */
	struct MapSpace_s *prev_light;

	/** Used to create chained light source list. */
	struct MapSpace_s *next_light;

	/** Tag for last_damage */
	uint32 round_tag;

	/** Counter for update tile */
	uint32 update_tile;

	/** Light source counter - the higher the brighter light source here */
	sint32 light_source;

	/**
	 * How much light is on this tile. 0 = total dark
	 * 255+ = full daylight. */
	sint32 light_value;

	/**
	 * Flags about this space
	 * @see map_look_flags */
	int flags;

	/** last_damage tmp backbuffer */
	uint16 last_damage;

	/** Terrain type flags (water, underwater,...) */
	uint16 move_flags;

	/** How much light this space provides */
	uint8 light;
} MapSpace;

/**
 * @defgroup map_flags Map flags
 * Map flags for global map settings, used in @ref mapstruct::map_flags.
 *@{*/
/** Map is outdoor map - daytime effects are on */
#define MAP_FLAG_OUTDOOR 1
/** Special unique map  */
#define MAP_FLAG_UNIQUE 2
/**
 * If true, reset time is not affected by players entering / exiting
 * map */
#define MAP_FLAG_FIXED_RTIME 4
/** No wizardy based spells */
#define MAP_FLAG_NOMAGIC 8
/** No prayer based spells */
#define MAP_FLAG_NOPRIEST 16
/** No harmful spells like fireball, magic bullet, etc. */
#define MAP_FLAG_NOHARM 32
/**
 * Don't allow any summoning spells. */
#define MAP_FLAG_NOSUMMON 64
/**
 * When set, a player login on this map will be forced to default
 * @ref mapstruct::enter_x and @ref mapstruct::enter_y of this map.
 * This avoids getting stuck in a map and treasure camping. */
#define MAP_FLAG_FIXED_LOGIN 128
/** Players cannot save on this map. */
#define MAP_FLAG_PLAYER_NO_SAVE 256
/** Unused. */
#define MAP_FLAG_UNUSED2 1024
/** Unused. */
#define MAP_FLAG_UNUSED3 2048
/** PvP is possible on this map. */
#define MAP_FLAG_PVP 4096
/** Don't save maps - only used with unique maps. */
#define MAP_FLAG_NO_SAVE 8192
/*@}*/

/**
 * @defgroup map_default_values Default map values
 * Nonzero default map values.
 *@{*/

/** Default darkness */
#define MAP_DEFAULT_DARKNESS	0
/*@}*/

#define SET_MAP_TILE_VISITED(m, x, y, id)                                                    \
{                                                                                            \
	if ((m)->pathfinding_id != (id))                                                         \
	{                                                                                        \
		(m)->pathfinding_id = (id);                                                          \
		memset((m)->bitmap, 0, ((MAP_WIDTH(m) + 31) / 32) * MAP_HEIGHT(m) * sizeof(uint32)); \
	}                                                                                        \
                                                                                             \
	(m)->bitmap[(x) / 32 + ((MAP_WIDTH(m) + 31) / 32) * (y)] |= (1U << ((x) % 32));          \
}

#define QUERY_MAP_TILE_VISITED(m, x, y, id) \
	((m)->pathfinding_id == (id) && ((m)->bitmap[(x) / 32 + ((MAP_WIDTH(m) + 31) / 32) * (y)] & (1U << ((x) % 32))))

/**
 * This is a game region.
 *
 * Each map is in a given region of the game world and links to a region
 * definiton. */
typedef struct regiondef
{
	/** Pointer to next region, NULL for the last one */
	struct regiondef *next;

	/**
	 * Pointer to the region that is a parent of the current
	 * region, if a value isn't defined in the current region
	 * we traverse this series of pointers until it is. */
	struct regiondef *parent;

	/** Shortend name of the region as maps refer to it */
	char *name;

	/**
	 * So that parent and child regions can be defined in
	 * any order, we keep hold of the parent_name during
	 * initialization, and the children get assigned to their
	 * parents later. (before runtime on the server though)
	 * nothing outside the init code should ever use this value. */
	char *parent_name;

	/**
	 * Official title of the region, this might be defined
	 * to be the same as name */
	char *longname;

	/** The description of the region. */
	char *msg;

	/** Where a player that is arrested in this region should be imprisoned. */
	char *jailmap;

	/** X coodinate in jailmap to which the player should be sent. */
	sint16 jailx;

	/** Y coodinate in jailmap to which the player should be sent. */
	sint16 jaily;
} region;

/**
 * A single map event, holding a pointer to map event object on map. */
typedef struct map_event
{
	/** Next map event in linked list. */
	struct map_event *next;

	/** Pointer to the actual map event. */
	object *event;

	/**
	 * Plugin the map event object is using. */
	struct _atrinik_plugin *plugin;
} map_event;

/**
 * In general, code should always use the macros above (or functions in
 * map.c) to access many of the values in the map structure. Failure to
 * do this will almost certainly break various features.
 *
 * You may think it is safe to look at width and height values directly
 * (or even through the macros), but doing so will completely break map
 * tiling. */
typedef struct mapdef
{
	/** Previous map before. If NULL we are first_map. */
	struct mapdef *previous;

	/** Next map, linked list */
	struct mapdef *next;

	/** Any maps tiled together to this one */
	struct mapdef *tile_map[TILED_MAPS];

	/** Name of map as given by its creator */
	char *name;

	/** Background music of the map */
	char *bg_music;

	/** Name of temporary file */
	char *tmpname;

	/** Message map creator may have left */
	char *msg;

	/** Filename of the map (shared string now) */
	shstr *path;

	/** Path to adjoining maps (shared strings) */
	shstr *tile_path[TILED_MAPS];

	/** Array of spaces on this map */
	MapSpace *spaces;

	/** List of tile spaces with light sources */
	MapSpace *first_light;

	/** Pointer to the region this map is in. */
	struct regiondef *region;

	/** Map-wide events for this map. */
	struct map_event *events;

	/** Linked list of linked lists of buttons */
	objectlink *buttons;

	/** Chained list of players on this map */
	object *player_first;

	/** Bitmap used for marking visited tiles in pathfinding */
	uint32 *bitmap;

	/** For which traversal is @ref mapstruct::bitmap valid. */
	uint32 pathfinding_id;

	/** Map flags for various map settings */
	uint32 map_flags;

	/** When this map should reset */
	uint32 reset_time;

	/**
	 * How many seconds must elapse before this map
	 * should be reset */
	uint32 reset_timeout;

	/** When this reaches 0, the map will be swapped out. */
	sint32 timeout;

	/** How long to wait before a map is swapped out. */
	uint32 swap_time;

	/**
	 * If not true, the map has been freed and must
	 * be loaded before used. The map, omap and map_ob
	 * arrays will be allocated when the map is loaded */
	uint32 in_memory;

	/** Used by relative_tile_position() to mark visited maps */
	uint32 traversed;

	/**
	 * Indicates the base light value on this map.
	 *
	 * This value is only used when the map is not marked as outdoor.
	 * @see MAP_DEFAULT_DARKNESS */
	int darkness;

	/**
	 * The real light_value, built out from darkness and possible other
	 * factors. */
	int light_value;

	/**
	 * What level the player should be to play here. Affects treasures,
	 * random shops and various other things. */
	int difficulty;

	/** Height of the map. */
	int height;

	/** Width of the map. */
	int width;

	/**
	 * Used to indicate the X position of where to put the player when he
	 * logs in to the map if the map has flag MAP_FLAG_FIXED_LOGIN set.
	 *
	 * Also used by exits as the default X location if the exit doesn't
	 * have one set.
	 * @see MAP_FLAG_FIXED_LOGIN */
	int enter_x;

	/**
	 * Used to indicate the Y position of where to put the player when he
	 * logs in to the map if the map has flag MAP_FLAG_FIXED_LOGIN set.
	 *
	 * Also used by exits as the default Y location if the exit doesn't
	 * have one set.
	 * @see MAP_FLAG_FIXED_LOGIN */
	int enter_y;

	/** Compression method used */
	int compressed;
} mapstruct;

/**
 * This is used by get_rangevector() to determine where the other
 * creature is. */
typedef struct rv_vector_s
{
	/** The distance away */
	unsigned int distance;

	/** X distance away */
	int distance_x;

	/** Y distance away */
	int distance_y;

	/** Atrinik direction scheme that the creature should head */
	int direction;

	/** Part of the object that is closest. */
	object *part;
} rv_vector;

/**
 * @defgroup range_vector_flags Range Vector Flags
 * Range vector flags, used by functions like get_rangevector() and
 * get_rangevector_from_mapcoords().
 *@{*/
#define RV_IGNORE_MULTIPART    0x01
#define RV_RECURSIVE_SEARCH    0x02

#define RV_MANHATTAN_DISTANCE  0x00
#define RV_EUCLIDIAN_DISTANCE  0x04
#define RV_DIAGONAL_DISTANCE   0x08
#define RV_NO_DISTANCE         (0x08 | 0x04)
/*@}*/

/**
 * @defgroup direction_constants Direction constants
 * The direction constants.
 *@{*/
/** North. */
#define NORTH 1
/** Northeast. */
#define NORTHEAST 2
/** East. */
#define EAST 3
/** Southeast. */
#define SOUTHEAST 4
/** South. */
#define SOUTH 5
/** Southwest. */
#define SOUTHWEST 6
/** West. */
#define WEST 7
/** Northwest. */
#define NORTHWEST 8
/*@}*/

#endif
